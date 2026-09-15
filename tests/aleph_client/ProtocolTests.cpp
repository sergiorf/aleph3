#include "aleph_client/Framing.hpp"
#include "aleph_client/Protocol.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using aleph3::client::FramingException;
using aleph3::client::InitializeParams;
using aleph3::client::ProtocolException;
using aleph3::client::RequestId;

namespace {
bool contains_forbidden_include(const std::filesystem::path& path, const std::vector<std::string>& forbidden) {
    std::ifstream input(path);
    REQUIRE(input);
    std::string line;
    while (std::getline(input, line)) {
        for (const auto& prefix : forbidden) {
            if (line.find("#include \"" + prefix) != std::string::npos ||
                line.find("#include <" + prefix) != std::string::npos) {
                return true;
            }
        }
    }
    return false;
}
}  // namespace

TEST_CASE("Aleph client encodes initialize and evaluate requests", "[aleph_client][protocol]") {
    const auto initialize = aleph3::client::encode_initialize_request(
        RequestId::number(1),
        InitializeParams{"aleph3-cli", "0.1.0", 1});

    REQUIRE(initialize.find(R"("method":"initialize")") != std::string::npos);
    REQUIRE(initialize.find(R"("client":"aleph3-cli")") != std::string::npos);
    REQUIRE(initialize.find(R"("protocolVersion":1)") != std::string::npos);

    const auto evaluate = aleph3::client::encode_evaluate_request(RequestId::string("cell-1"), "1/2 + 1/3");

    REQUIRE(evaluate.find(R"("method":"evaluate")") != std::string::npos);
    REQUIRE(evaluate.find(R"("id":"cell-1")") != std::string::npos);
    REQUIRE(evaluate.find(R"("source":"1/2 + 1/3")") != std::string::npos);
}

TEST_CASE("Aleph client decodes evaluation success and preserves exact text", "[aleph_client][protocol]") {
    const auto response = aleph3::client::decode_response(
        R"({"jsonrpc":"2.0","id":7,"result":{"ok":true,"source":"1/2 + 1/3","representations":{"text/plain":"5/6","text/x-aleph-source":"5/6"},"diagnostics":[]}})");

    REQUIRE(response.ok());
    REQUIRE(response.id == RequestId::number(7));
    REQUIRE(response.evaluation.has_value());
    REQUIRE(response.evaluation->ok);
    REQUIRE(response.evaluation->representations.at("text/plain") == "5/6");
}

TEST_CASE("Aleph client decodes protocol errors and diagnostics", "[aleph_client][protocol]") {
    const auto response = aleph3::client::decode_response(
        R"({"jsonrpc":"2.0","id":"req","error":{"code":"kernel.division_by_zero","message":"Division by zero is not allowed.","diagnostics":[{"code":"kernel.division_by_zero","severity":"error","message":"Division by zero is not allowed."}]}})");

    REQUIRE_FALSE(response.ok());
    REQUIRE(response.id == RequestId::string("req"));
    REQUIRE(response.error.has_value());
    REQUIRE(response.error->code == "kernel.division_by_zero");
    REQUIRE(response.error->diagnostics.size() == 1);
}

TEST_CASE("Aleph client rejects malformed response envelopes", "[aleph_client][protocol]") {
    REQUIRE_THROWS_AS(aleph3::client::decode_response("{"), ProtocolException);
    REQUIRE_THROWS_AS(aleph3::client::decode_response(R"({"jsonrpc":"2.0","id":1})"), ProtocolException);
    REQUIRE_THROWS_AS(
        aleph3::client::decode_response(R"({"jsonrpc":"2.0","id":1,"result":{"ok":true}})"),
        ProtocolException);
}

TEST_CASE("Aleph client frames and reads adjacent messages", "[aleph_client][framing]") {
    const auto first = aleph3::client::write_frame(R"({"jsonrpc":"2.0","id":1})");
    const auto second = aleph3::client::write_frame(R"({"jsonrpc":"2.0","id":2})");

    const auto frames = aleph3::client::read_frames(first + second);

    REQUIRE(frames.size() == 2);
    REQUIRE(frames[0] == R"({"jsonrpc":"2.0","id":1})");
    REQUIRE(frames[1] == R"({"jsonrpc":"2.0","id":2})");
}

TEST_CASE("Aleph client rejects invalid frames", "[aleph_client][framing]") {
    REQUIRE_THROWS_AS(aleph3::client::read_frames("Content-Length: x\r\n\r\n{}"), FramingException);
    REQUIRE_THROWS_AS(aleph3::client::read_frames("Content-Length: 10\r\n\r\n{}"), FramingException);
    REQUIRE_THROWS_AS(aleph3::client::read_frames("Other: 2\r\n\r\n{}"), FramingException);
    REQUIRE_THROWS_AS(aleph3::client::read_frames("Content-Length: 3\r\n\r\nabc", 2), FramingException);
}

TEST_CASE("Aleph client public layer does not include private semantic headers", "[aleph_client][boundary]") {
    const auto source_root = std::filesystem::path(ALEPH3_SOURCE_DIR);
    const std::vector<std::filesystem::path> files = {
        source_root / "include/aleph_client/Framing.hpp",
        source_root / "include/aleph_client/Protocol.hpp",
        source_root / "include/aleph_client/RuntimeClient.hpp",
        source_root / "src/aleph_client/Framing.cpp",
        source_root / "src/aleph_client/Protocol.cpp",
        source_root / "src/aleph_client/RuntimeClient.cpp",
        source_root / "tests/aleph_client/FakeRuntime.cpp",
        source_root / "tests/aleph_client/RuntimeClientTests.cpp"};
    const std::vector<std::string> forbidden = {
        "algebra/",
        "evaluator/",
        "expr/",
        "frontend/",
        "ir/",
        "kernel/",
        "normalizer/",
        "packs/",
        "parser/",
        "sdk/",
        "session/",
        "symbols/",
        "tooling/",
        "transforms/",
        "web/",
        "notebook/"};

    for (const auto& file : files) {
        REQUIRE_FALSE(contains_forbidden_include(file, forbidden));
    }
}
