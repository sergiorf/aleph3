#include "web/EngineApi.hpp"

#include "json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>

namespace {

using Json = nlohmann::json;

Json body_json(const aleph3::web::ApiResponse& response) {
    return Json::parse(response.body);
}

struct Harness {
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::time_point{};
    int next_id = 0;
    aleph3::web::EngineApi api{
        aleph3::web::EngineApiLimits{},
        [&] { return now; },
        [&] {
            ++next_id;
            return "engine-id-" + std::to_string(next_id);
        }};

    std::string create_session() {
        const auto response = api.handle({"POST", "/internal/sessions", {}, ""});
        REQUIRE(response.status == 201);
        return body_json(response).at("sessionId").get<std::string>();
    }

    aleph3::web::ApiResponse evaluate(const std::string& session_id, const std::string& source) {
        return api.handle({
            "POST",
            "/internal/sessions/" + session_id + "/evaluate",
            {},
            Json{{"source", source}}.dump()});
    }
};

}  // namespace

TEST_CASE("Engine API exposes internal health and creates sessions", "[web][engine]") {
    Harness harness;

    const auto health = body_json(harness.api.handle({"GET", "/internal/health", {}, ""}));
    REQUIRE(health.at("status") == "ok");
    REQUIRE(health.at("service") == "aleph3-engine");
    REQUIRE(health.at("ready") == true);

    const auto session_id = harness.create_session();
    REQUIRE(session_id == "engine-id-1");
    REQUIRE(harness.api.active_session_count() == 1);
}

TEST_CASE("Engine API evaluates through the shared symbolic session", "[web][engine][session]") {
    Harness harness;
    const auto session_id = harness.create_session();

    const auto exact = body_json(harness.evaluate(session_id, "1/2 + 1/3"));
    REQUIRE(exact.at("status") == "ok");
    REQUIRE(exact.at("result").at("status") == "ok");
    REQUIRE(exact.at("result").at("canonicalText") == "5/6");

    const auto algebra = body_json(harness.evaluate(session_id, "Factor[x^2 - 1]"));
    REQUIRE(algebra.at("result").at("status") == "ok");
    REQUIRE(algebra.at("result").at("canonicalText").get<std::string>().find("x - 1") != std::string::npos);
}

TEST_CASE("Engine API exposes reset without unloading registered packs", "[web][engine][reset]") {
    Harness harness;
    const auto session_id = harness.create_session();

    const auto reset = body_json(harness.api.handle({"POST", "/internal/sessions/" + session_id + "/reset", {}, ""}));
    REQUIRE(reset.at("status") == "ok");
    REQUIRE(reset.at("reset") == true);

    REQUIRE(body_json(harness.evaluate(session_id, "Factor[x^2 - 1]")).at("result").at("status") == "ok");
}

TEST_CASE("Engine API returns diagnostics and stable request errors", "[web][engine][diagnostics]") {
    Harness harness;
    const auto session_id = harness.create_session();

    const auto diagnostic = body_json(harness.evaluate(session_id, "("));
    REQUIRE(diagnostic.at("status") == "ok");
    REQUIRE(diagnostic.at("result").at("status") == "error");
    REQUIRE(diagnostic.at("result").at("canonicalText").is_null());
    REQUIRE_FALSE(diagnostic.at("result").at("diagnostics").empty());

    const auto invalid = harness.api.handle({"POST", "/internal/sessions/" + session_id + "/evaluate", {}, R"({"source":1})"});
    REQUIRE(invalid.status == 400);
    REQUIRE(body_json(invalid).at("error").at("code") == "engine.invalid_request");

    const auto missing = harness.api.handle({"POST", "/internal/sessions/missing/evaluate", {}, R"({"source":"1"})"});
    REQUIRE(missing.status == 404);
    REQUIRE(body_json(missing).at("error").at("code") == "engine.unknown_session");
}

TEST_CASE("Engine API enforces request and source limits", "[web][engine][limits]") {
    aleph3::web::EngineApiLimits request_limits;
    request_limits.max_request_body_bytes = 16;
    aleph3::web::EngineApi request_api{request_limits, {}, [] { return "limited-session"; }};

    const auto session = body_json(request_api.handle({"POST", "/internal/sessions", {}, ""})).at("sessionId").get<std::string>();

    const auto body_too_large = request_api.handle({"POST", "/internal/sessions/" + session + "/evaluate", {}, R"({"source":"123456789"})"});
    REQUIRE(body_too_large.status == 413);
    REQUIRE(body_json(body_too_large).at("error").at("code") == "engine.request_too_large");

    aleph3::web::EngineApiLimits source_limits;
    source_limits.max_evaluate_source_bytes = 4;
    aleph3::web::EngineApi source_api{source_limits, {}, [] { return "source-limited-session"; }};
    const auto source_session = body_json(source_api.handle({"POST", "/internal/sessions", {}, ""})).at("sessionId").get<std::string>();

    const auto source_too_large = source_api.handle({"POST", "/internal/sessions/" + source_session + "/evaluate", {}, R"({"source":"12345"})"});
    REQUIRE(source_too_large.status == 413);
    REQUIRE(body_json(source_too_large).at("error").at("code") == "engine.source_too_large");
}
