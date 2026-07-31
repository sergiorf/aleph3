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
};

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
