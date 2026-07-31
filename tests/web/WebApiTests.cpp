#include "web/WebApi.hpp"

#include "json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>
#include <utility>

using aleph3::web::ApiRequest;
using aleph3::web::WebApi;

namespace {

nlohmann::json body_json(const aleph3::web::ApiResponse& response) {
    return nlohmann::json::parse(response.body);
}

class ApiHarness {
public:
    WebApi api{
        {},
        [this] { return now; },
        [this] { return "id-" + std::to_string(++next_id); }};
    std::chrono::steady_clock::time_point now{};
    int next_id = 0;

    std::string create_client() {
        const auto response = api.handle({"POST", "/api/clients", {}, ""});
        REQUIRE(response.status == 201);
        return body_json(response).at("anonymousClientId").get<std::string>();
    }

    std::string create_session(const std::string& client_id) {
        const auto response = api.handle({"POST", "/api/sessions", {{"X-Aleph3-Client", client_id}}, ""});
        REQUIRE(response.status == 201);
        return body_json(response).at("sessionId").get<std::string>();
    }

    aleph3::web::ApiResponse evaluate(
        const std::string& client_id,
        const std::string& session_id,
        const std::string& source) {
        return api.handle({
            "POST",
            "/api/sessions/" + session_id + "/evaluate",
            {{"X-Aleph3-Client", client_id}},
            nlohmann::json{{"source", source}}.dump()});
    }

    aleph3::web::ApiResponse complete(
        const std::string& client_id,
        const std::string& session_id,
        const std::string& prefix) {
        return api.handle({
            "GET",
            "/api/sessions/" + session_id + "/complete?prefix=" + prefix,
            {{"X-Aleph3-Client", client_id}},
            ""});
    }

    aleph3::web::ApiResponse help(
        const std::string& client_id,
        const std::string& session_id,
        const std::string& query) {
        return api.handle({
            "GET",
            "/api/sessions/" + session_id + "/help?query=" + query,
            {{"X-Aleph3-Client", client_id}},
            ""});
    }

    std::string create_notebook(const std::string& client_id, nlohmann::json document, const std::string& title = "Scratch") {
        const auto response = api.handle({
            "POST",
            "/api/notebooks",
            {{"X-Aleph3-Client", client_id}},
            nlohmann::json{{"title", title}, {"document", std::move(document)}}.dump()});
        REQUIRE(response.status == 201);
        return body_json(response).at("notebook").at("id").get<std::string>();
    }
};

nlohmann::json minimal_document(const std::string& source = "1 + 1") {
    return {
        {"format", "aleph3-notebook"},
        {"version", 1},
        {"cells", nlohmann::json::array({
            {{"id", "cell-1"}, {"kind", "input"}, {"source", source}}
        })}
    };
}

nlohmann::json multi_cell_document() {
    return {
        {"format", "aleph3-notebook"},
        {"version", 1},
        {"cells", nlohmann::json::array({
            {{"id", "notes"}, {"kind", "text"}, {"source", "Definitions flow downward."}},
            {{"id", "define"}, {"kind", "input"}, {"source", "a = 2"}},
            {{"id", "use"}, {"kind", "input"}, {"source", "a + 3"}},
            {{"id", "bad"}, {"kind", "input"}, {"source", "("}},
            {{"id", "after"}, {"kind", "input"}, {"source", "1 + 1"}}
        })},
        {"results", nlohmann::json::array({
            {{"source_cell_id", "define"}, {"ok", true}, {"output", "stale"}, {"diagnostics", nlohmann::json::array()}, {"producer_version", "old"}}
        })}
    };
}

}  // namespace

TEST_CASE("Web API health endpoint returns a stable JSON envelope", "[web][api]") {
    WebApi api;

    const auto response = api.handle({"GET", "/api/health", {}, ""});

    REQUIRE(response.status == 200);
    const auto body = body_json(response);
    REQUIRE(body.at("status") == "ok");
    REQUIRE(body.at("service") == "aleph3-web-api");
    REQUIRE(body.at("ready") == true);
}

TEST_CASE("Web API creates anonymous clients and sessions", "[web][api]") {
    ApiHarness harness;

    const auto client_id = harness.create_client();
    const auto session_id = harness.create_session(client_id);

    REQUIRE(client_id == "id-1");
    REQUIRE(session_id == "id-2");
    REQUIRE(harness.api.active_session_count() == 1);
}

TEST_CASE("Web API evaluates expressions through isolated sessions", "[web][api][session]") {
    ApiHarness harness;
    const auto left_client = harness.create_client();
    const auto right_client = harness.create_client();
    const auto left_session = harness.create_session(left_client);
    const auto right_session = harness.create_session(right_client);

    REQUIRE(body_json(harness.evaluate(left_client, left_session, "a = 2")).at("result").at("status") == "ok");

    const auto left_value = body_json(harness.evaluate(left_client, left_session, "a + 3"));
    REQUIRE(left_value.at("result").at("status") == "ok");
    REQUIRE(left_value.at("result").at("canonicalText") == "5");

    const auto right_value = body_json(harness.evaluate(right_client, right_session, "a"));
    REQUIRE(right_value.at("result").at("status") == "ok");
    REQUIRE(right_value.at("result").at("canonicalText") == "a");
}

TEST_CASE("Web API reset clears session-local definitions", "[web][api][session]") {
    ApiHarness harness;
    const auto client_id = harness.create_client();
    const auto session_id = harness.create_session(client_id);
    REQUIRE(body_json(harness.evaluate(client_id, session_id, "a = 2")).at("result").at("status") == "ok");

    const auto reset = harness.api.handle({
        "POST",
        "/api/sessions/" + session_id + "/reset",
        {{"X-Aleph3-Client", client_id}},
        ""});
    REQUIRE(reset.status == 200);

    const auto value = body_json(harness.evaluate(client_id, session_id, "a"));
    REQUIRE(value.at("result").at("status") == "ok");
    REQUIRE(value.at("result").at("canonicalText") == "a");
}

TEST_CASE("Web API exposes session-backed completion metadata", "[web][api][session][completion]") {
    ApiHarness harness;
    const auto client_id = harness.create_client();
    const auto session_id = harness.create_session(client_id);

    const auto pack_completion = body_json(harness.complete(client_id, session_id, "Fac"));
    REQUIRE(pack_completion.at("status") == "ok");
    REQUIRE(pack_completion.at("prefix") == "Fac");
    REQUIRE(pack_completion.at("completions").size() == 1);
    REQUIRE(pack_completion.at("completions").at(0).at("name") == "Factor");
    REQUIRE(pack_completion.at("completions").at(0).at("category") == "pack");
    REQUIRE(pack_completion.at("completions").at(0).at("owningPackage") == "core-algebra");
    REQUIRE(pack_completion.at("completions").at(0).at("documentation").get<std::string>().find("Factor") != std::string::npos);

    REQUIRE(body_json(harness.evaluate(client_id, session_id, "localWebValue = 2")).at("result").at("status") == "ok");
    const auto local_completion = body_json(harness.complete(client_id, session_id, "localWeb"));
    REQUIRE(local_completion.at("completions").size() == 1);
    REQUIRE(local_completion.at("completions").at(0).at("name") == "localWebValue");
    REQUIRE(local_completion.at("completions").at(0).at("category") == "symbol");
    REQUIRE(local_completion.at("completions").at(0).at("documentation") == "session-local own value");
}

TEST_CASE("Web API exposes focused help from the shared session catalog", "[web][api][session][help]") {
    ApiHarness harness;
    const auto client_id = harness.create_client();
    const auto session_id = harness.create_session(client_id);

    const auto factor_help = body_json(harness.help(client_id, session_id, "Factor"));
    REQUIRE(factor_help.at("status") == "ok");
    REQUIRE(factor_help.at("query") == "Factor");
    REQUIRE(factor_help.at("entries").size() == 1);
    REQUIRE(factor_help.at("entries").at(0).at("name") == "Factor");
    REQUIRE(factor_help.at("entries").at(0).at("category") == "pack");
    REQUIRE(factor_help.at("entries").at(0).at("owningPackage") == "core-algebra");
    REQUIRE_FALSE(factor_help.at("entries").at(0).at("forms").empty());
    REQUIRE_FALSE(factor_help.at("entries").at(0).at("examples").empty());
    REQUIRE_FALSE(factor_help.at("entries").at(0).at("unsupported").get<std::string>().empty());

    REQUIRE(body_json(harness.evaluate(client_id, session_id, "localHelpWeb[x_] := x + 1")).at("result").at("status") == "ok");
    const auto local_help = body_json(harness.help(client_id, session_id, "localHelpWeb"));
    REQUIRE(local_help.at("entries").size() == 1);
    REQUIRE(local_help.at("entries").at(0).at("name") == "localHelpWeb");
    REQUIRE(local_help.at("entries").at(0).at("category") == "function");
    REQUIRE(local_help.at("entries").at(0).at("description") == "session-local user function");

    const auto package_help = body_json(harness.help(client_id, session_id, "core-calculus"));
    REQUIRE(package_help.at("entries").size() >= 1);
    REQUIRE(package_help.at("entries").at(0).contains("manualAnchor"));

    const auto missing_help = harness.help(client_id, session_id, "NoSuchWebHelpPrefix");
    REQUIRE(missing_help.status == 404);
    REQUIRE(body_json(missing_help).at("error").at("code") == "web.help_not_found");
}

TEST_CASE("Web API discovery routes decode query parameters and preserve ownership checks", "[web][api][session][completion][help]") {
    ApiHarness harness;
    const auto left_client = harness.create_client();
    const auto right_client = harness.create_client();
    const auto left_session = harness.create_session(left_client);

    const auto plus_help = body_json(harness.help(left_client, left_session, "Replace%41ll"));
    REQUIRE(plus_help.at("entries").size() == 1);
    REQUIRE(plus_help.at("entries").at(0).at("name") == "ReplaceAll");

    const auto forbidden = harness.complete(right_client, left_session, "Fac");
    REQUIRE(forbidden.status == 403);
    REQUIRE(body_json(forbidden).at("error").at("code") == "web.session_forbidden");
}

TEST_CASE("Web API returns structured diagnostics and request errors", "[web][api][diagnostics]") {
    ApiHarness harness;
    const auto client_id = harness.create_client();
    const auto session_id = harness.create_session(client_id);

    const auto diagnostic = body_json(harness.evaluate(client_id, session_id, "("));
    REQUIRE(diagnostic.at("status") == "ok");
    REQUIRE(diagnostic.at("result").at("status") == "error");
    REQUIRE(diagnostic.at("result").at("canonicalText").is_null());
    REQUIRE(diagnostic.at("result").at("diagnostics").at(0).at("code") == "session.parse_error");

    const auto missing_client = harness.api.handle({"POST", "/api/sessions", {}, ""});
    REQUIRE(missing_client.status == 401);
    REQUIRE(body_json(missing_client).at("error").at("code") == "web.missing_client");

    const auto invalid_body = harness.api.handle({
        "POST",
        "/api/sessions/" + session_id + "/evaluate",
        {{"X-Aleph3-Client", client_id}},
        "{\"source\": 12}"});
    REQUIRE(invalid_body.status == 400);
    REQUIRE(body_json(invalid_body).at("error").at("code") == "web.invalid_request");

    const auto malformed_json = harness.api.handle({
        "POST",
        "/api/sessions/" + session_id + "/evaluate",
        {{"X-Aleph3-Client", client_id}},
        "{"});
    REQUIRE(malformed_json.status == 400);
    REQUIRE(body_json(malformed_json).at("error").at("code") == "web.invalid_json");
}

TEST_CASE("Web API enforces ownership, quotas, and idle expiration", "[web][api][limits]") {
    aleph3::web::ApiLimits limits;
    limits.max_sessions_per_client = 1;
    limits.session_idle_ttl = std::chrono::minutes{1};

    std::chrono::steady_clock::time_point now{};
    int next_id = 0;
    WebApi api{
        limits,
        [&] { return now; },
        [&] { return "id-" + std::to_string(++next_id); }};

    const auto first_client_response = api.handle({"POST", "/api/clients", {}, ""});
    const auto first_client = body_json(first_client_response).at("anonymousClientId").get<std::string>();
    const auto second_client_response = api.handle({"POST", "/api/clients", {}, ""});
    const auto second_client = body_json(second_client_response).at("anonymousClientId").get<std::string>();

    const auto session_response = api.handle({"POST", "/api/sessions", {{"X-Aleph3-Client", first_client}}, ""});
    const auto session_id = body_json(session_response).at("sessionId").get<std::string>();

    const auto quota = api.handle({"POST", "/api/sessions", {{"X-Aleph3-Client", first_client}}, ""});
    REQUIRE(quota.status == 429);

    const auto forbidden = api.handle({
        "POST",
        "/api/sessions/" + session_id + "/evaluate",
        {{"X-Aleph3-Client", second_client}},
        nlohmann::json{{"source", "1 + 1"}}.dump()});
    REQUIRE(forbidden.status == 403);

    now += std::chrono::minutes{2};
    const auto expired = api.handle({
        "POST",
        "/api/sessions/" + session_id + "/evaluate",
        {{"X-Aleph3-Client", first_client}},
        nlohmann::json{{"source", "1 + 1"}}.dump()});
    REQUIRE(expired.status == 404);
}

TEST_CASE("Web API enforces request and source size limits", "[web][api][limits]") {
    aleph3::web::ApiLimits limits;
    limits.max_request_body_bytes = 8;
    limits.max_evaluate_source_bytes = 3;

    std::chrono::steady_clock::time_point now{};
    int next_id = 0;
    WebApi api{
        limits,
        [&] { return now; },
        [&] { return "id-" + std::to_string(++next_id); }};

    const auto client_response = api.handle({"POST", "/api/clients", {}, ""});
    const auto client_id = body_json(client_response).at("anonymousClientId").get<std::string>();
    const auto session_response = api.handle({"POST", "/api/sessions", {{"X-Aleph3-Client", client_id}}, ""});
    const auto session_id = body_json(session_response).at("sessionId").get<std::string>();

    const auto oversized_body = api.handle({
        "POST",
        "/api/sessions/" + session_id + "/evaluate",
        {{"X-Aleph3-Client", client_id}},
        "123456789"});
    REQUIRE(oversized_body.status == 413);
    REQUIRE(body_json(oversized_body).at("error").at("code") == "web.request_too_large");

    limits.max_request_body_bytes = 1024;
    WebApi source_limited_api{
        limits,
        [&] { return now; },
        [&] { return "source-id-" + std::to_string(++next_id); }};
    const auto source_client_response = source_limited_api.handle({"POST", "/api/clients", {}, ""});
    const auto source_client_id = body_json(source_client_response).at("anonymousClientId").get<std::string>();
    const auto source_session_response = source_limited_api.handle({
        "POST",
        "/api/sessions",
        {{"X-Aleph3-Client", source_client_id}},
        ""});
    const auto source_session_id = body_json(source_session_response).at("sessionId").get<std::string>();

    const auto oversized_source = source_limited_api.handle({
        "POST",
        "/api/sessions/" + source_session_id + "/evaluate",
        {{"X-Aleph3-Client", source_client_id}},
        nlohmann::json{{"source", "1 + 1"}}.dump()});
    REQUIRE(oversized_source.status == 413);
    REQUIRE(body_json(oversized_source).at("error").at("code") == "web.source_too_large");
}

TEST_CASE("Web API exposes notebook CRUD through the notebook store boundary", "[web][api][notebook]") {
    ApiHarness harness;
    const auto client_id = harness.create_client();

    const auto create = harness.api.handle({
        "POST",
        "/api/notebooks",
        {{"X-Aleph3-Client", client_id}},
        nlohmann::json{{"title", "Scratch"}, {"document", minimal_document("1/2 + 1/3")}}.dump()});
    REQUIRE(create.status == 201);
    const auto created = body_json(create).at("notebook");
    const auto notebook_id = created.at("id").get<std::string>();
    REQUIRE(created.at("title") == "Scratch");
    REQUIRE(created.at("document").at("cells").at(0).at("source") == "1/2 + 1/3");

    const auto list = body_json(harness.api.handle({"GET", "/api/notebooks", {{"X-Aleph3-Client", client_id}}, ""}));
    REQUIRE(list.at("notebooks").size() == 1);
    REQUIRE(list.at("notebooks").at(0).at("id") == notebook_id);

    const auto update = harness.api.handle({
        "PUT",
        "/api/notebooks/" + notebook_id,
        {{"X-Aleph3-Client", client_id}},
        nlohmann::json{{"title", "Updated"}, {"document", minimal_document("Expand[(x + 1)^2]")}}.dump()});
    REQUIRE(update.status == 200);
    REQUIRE(body_json(update).at("notebook").at("title") == "Updated");

    const auto load = harness.api.handle({"GET", "/api/notebooks/" + notebook_id, {{"X-Aleph3-Client", client_id}}, ""});
    REQUIRE(load.status == 200);
    const auto loaded = body_json(load).at("notebook");
    REQUIRE(loaded.at("title") == "Updated");
    REQUIRE(loaded.at("document").at("cells").at(0).at("source") == "Expand[(x + 1)^2]");

    const auto remove = harness.api.handle({"DELETE", "/api/notebooks/" + notebook_id, {{"X-Aleph3-Client", client_id}}, ""});
    REQUIRE(remove.status == 204);
    const auto missing = harness.api.handle({"GET", "/api/notebooks/" + notebook_id, {{"X-Aleph3-Client", client_id}}, ""});
    REQUIRE(missing.status == 404);
}

TEST_CASE("Web API rejects cross-client notebook access", "[web][api][notebook][ownership]") {
    ApiHarness harness;
    const auto left_client = harness.create_client();
    const auto right_client = harness.create_client();
    const auto create = harness.api.handle({
        "POST",
        "/api/notebooks",
        {{"X-Aleph3-Client", left_client}},
        nlohmann::json{{"title", "Private"}, {"document", minimal_document()}}.dump()});
    REQUIRE(create.status == 201);
    const auto notebook_id = body_json(create).at("notebook").at("id").get<std::string>();

    const auto forbidden = harness.api.handle({
        "GET",
        "/api/notebooks/" + notebook_id,
        {{"X-Aleph3-Client", right_client}},
        ""});
    REQUIRE(forbidden.status == 403);
    REQUIRE(body_json(forbidden).at("error").at("code") == "web.notebook_forbidden");
}

TEST_CASE("Web API validates notebook documents and storage quotas", "[web][api][notebook][limits]") {
    aleph3::web::ApiLimits limits;
    limits.max_notebook_document_bytes = 256;
    limits.max_notebooks_per_client = 1;
    limits.max_stored_notebook_bytes_per_client = 1024;
    WebApi api{limits, {}, [] {
                   static int id = 0;
                   return "quota-id-" + std::to_string(++id);
               }};

    const auto client_response = api.handle({"POST", "/api/clients", {}, ""});
    const auto client_id = body_json(client_response).at("anonymousClientId").get<std::string>();

    const auto invalid = api.handle({
        "POST",
        "/api/notebooks",
        {{"X-Aleph3-Client", client_id}},
        nlohmann::json{{"title", "Bad"}, {"document", nlohmann::json{{"format", "wrong"}, {"version", 1}, {"cells", nlohmann::json::array()}}}}.dump()});
    REQUIRE(invalid.status == 400);
    REQUIRE(body_json(invalid).at("error").at("code") == "notebook.unsupported_format");

    const auto oversized = api.handle({
        "POST",
        "/api/notebooks",
        {{"X-Aleph3-Client", client_id}},
        nlohmann::json{{"title", "Oversized"}, {"document", minimal_document(std::string(300, 'x'))}}.dump()});
    REQUIRE(oversized.status == 413);
    REQUIRE(body_json(oversized).at("error").at("code") == "notebook.limit_exceeded");

    const auto create = api.handle({
        "POST",
        "/api/notebooks",
        {{"X-Aleph3-Client", client_id}},
        nlohmann::json{{"title", "One"}, {"document", minimal_document()}}.dump()});
    REQUIRE(create.status == 201);

    const auto quota = api.handle({
        "POST",
        "/api/notebooks",
        {{"X-Aleph3-Client", client_id}},
        nlohmann::json{{"title", "Two"}, {"document", minimal_document("2 + 2")}}.dump()});
    REQUIRE(quota.status == 429);
    REQUIRE(body_json(quota).at("error").at("code") == "web.notebook_quota_exceeded");

    limits.max_notebook_document_bytes = 4096;
    limits.max_stored_notebook_bytes_per_client = 512;
    WebApi storage_limited_api{limits, {}, [] {
                                  static int id = 1000;
                                  return "storage-id-" + std::to_string(++id);
                              }};
    const auto storage_client_response = storage_limited_api.handle({"POST", "/api/clients", {}, ""});
    const auto storage_client_id = body_json(storage_client_response).at("anonymousClientId").get<std::string>();
    const auto storage_create = storage_limited_api.handle({
        "POST",
        "/api/notebooks",
        {{"X-Aleph3-Client", storage_client_id}},
        nlohmann::json{{"title", "One"}, {"document", minimal_document()}}.dump()});
    REQUIRE(storage_create.status == 201);
    const auto storage_notebook_id = body_json(storage_create).at("notebook").at("id").get<std::string>();
    const auto storage_quota = storage_limited_api.handle({
        "PUT",
        "/api/notebooks/" + storage_notebook_id,
        {{"X-Aleph3-Client", storage_client_id}},
        nlohmann::json{{"title", "Large"}, {"document", minimal_document(std::string(800, 'x'))}}.dump()});
    REQUIRE(storage_quota.status == 429);
    REQUIRE(body_json(storage_quota).at("error").at("code") == "web.notebook_storage_quota_exceeded");
}

TEST_CASE("Web API runs persisted notebooks through a clean notebook runner", "[web][api][notebook][runner]") {
    ApiHarness harness;
    const auto client_id = harness.create_client();
    const auto notebook_id = harness.create_notebook(client_id, multi_cell_document());

    const auto run = harness.api.handle({
        "POST",
        "/api/notebooks/" + notebook_id + "/run-all",
        {{"X-Aleph3-Client", client_id}},
        ""});

    REQUIRE(run.status == 200);
    const auto document = body_json(run).at("notebook").at("document");
    REQUIRE(document.at("results").size() == 4);
    REQUIRE(document.at("results").at(0).at("source_cell_id") == "define");
    REQUIRE(document.at("results").at(1).at("source_cell_id") == "use");
    REQUIRE(document.at("results").at(1).at("output") == "5");
    REQUIRE_FALSE(document.at("results").at(2).at("ok").get<bool>());
    REQUIRE(document.at("results").at(2).at("diagnostics").at(0).at("code") == "session.parse_error");
    REQUIRE(document.at("results").at(3).at("output") == "2");

    const auto load = body_json(harness.api.handle({"GET", "/api/notebooks/" + notebook_id, {{"X-Aleph3-Client", client_id}}, ""}));
    REQUIRE(load.at("notebook").at("document").at("results").at(1).at("output") == "5");
}

TEST_CASE("Web API clears persisted notebook generated results", "[web][api][notebook][runner]") {
    ApiHarness harness;
    const auto client_id = harness.create_client();
    const auto notebook_id = harness.create_notebook(client_id, multi_cell_document());

    const auto clear = harness.api.handle({
        "POST",
        "/api/notebooks/" + notebook_id + "/clear-results",
        {{"X-Aleph3-Client", client_id}},
        ""});

    REQUIRE(clear.status == 200);
    const auto document = body_json(clear).at("notebook").at("document");
    REQUIRE_FALSE(document.contains("results"));

    const auto load = body_json(harness.api.handle({"GET", "/api/notebooks/" + notebook_id, {{"X-Aleph3-Client", client_id}}, ""}));
    REQUIRE_FALSE(load.at("notebook").at("document").contains("results"));
}

TEST_CASE("Web API rejects cross-client notebook runner operations", "[web][api][notebook][ownership]") {
    ApiHarness harness;
    const auto left_client = harness.create_client();
    const auto right_client = harness.create_client();
    const auto notebook_id = harness.create_notebook(left_client, minimal_document());

    const auto forbidden = harness.api.handle({
        "POST",
        "/api/notebooks/" + notebook_id + "/run-all",
        {{"X-Aleph3-Client", right_client}},
        ""});

    REQUIRE(forbidden.status == 403);
    REQUIRE(body_json(forbidden).at("error").at("code") == "web.notebook_forbidden");
}

TEST_CASE("Web API lists and copies verified example notebooks", "[web][api][examples]") {
    ApiHarness harness;
    const auto client_id = harness.create_client();

    const auto list = harness.api.handle({"GET", "/api/examples", {{"X-Aleph3-Client", client_id}}, ""});
    REQUIRE(list.status == 200);
    const auto examples = body_json(list).at("examples");
    REQUIRE(examples.size() >= 1);
    const auto example_id = examples.at(0).at("id").get<std::string>();
    REQUIRE_FALSE(examples.at(0).at("title").get<std::string>().empty());

    const auto copy = harness.api.handle({
        "POST",
        "/api/examples/" + example_id + "/copy",
        {{"X-Aleph3-Client", client_id}},
        ""});
    REQUIRE(copy.status == 201);
    const auto notebook = body_json(copy).at("notebook");
    REQUIRE(notebook.at("title") == examples.at(0).at("title"));
    REQUIRE(notebook.at("document").at("cells").size() >= 1);

    const auto run = harness.api.handle({
        "POST",
        "/api/notebooks/" + notebook.at("id").get<std::string>() + "/run-all",
        {{"X-Aleph3-Client", client_id}},
        ""});
    REQUIRE(run.status == 200);
    const auto results = body_json(run).at("notebook").at("document").at("results");
    REQUIRE(results.size() == 10);
    REQUIRE(results.at(0).at("output") == "5/6");
    REQUIRE(results.at(2).at("output") == "5");
    REQUIRE(results.at(9).at("diagnostics").at(0).at("code") == "session.parse_error");

    const auto missing = harness.api.handle({
        "POST",
        "/api/examples/no-such-example/copy",
        {{"X-Aleph3-Client", client_id}},
        ""});
    REQUIRE(missing.status == 404);
    REQUIRE(body_json(missing).at("error").at("code") == "web.unknown_example");
}
