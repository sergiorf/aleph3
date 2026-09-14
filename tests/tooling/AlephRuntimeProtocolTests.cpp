#include "aleph_client/Framing.hpp"
#include "aleph_client/Protocol.hpp"
#include "tooling/RuntimeProtocolServer.hpp"

#include "json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

using aleph3::client::RequestId;

namespace {
using Json = nlohmann::json;

Json request(int id, std::string method, Json params = Json::object()) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"method", std::move(method)},
        {"params", std::move(params)}};
}

Json response_json(const std::string& payload) {
    return Json::parse(payload);
}

std::filesystem::path test_file(const std::string& name) {
    auto path = std::filesystem::temp_directory_path() / ("aleph3_runtime_" + name);
    std::filesystem::remove(path);
    return path;
}

std::string runtime_command(const std::filesystem::path& input_path, const std::filesystem::path& output_path) {
    const auto runtime_path = std::filesystem::current_path() / ALEPH_RUNTIME_EXE_NAME;
#ifdef _WIN32
    return "cmd /C \"\"" + runtime_path.string() + "\" < \"" + input_path.string() + "\" > \"" +
        output_path.string() + "\"\"";
#else
    return "\"" + runtime_path.string() + "\" < \"" + input_path.string() + "\" > \"" + output_path.string() + "\"";
#endif
}

std::vector<std::string> run_runtime(const std::string& input) {
    const auto input_path = test_file("input.txt");
    const auto output_path = test_file("output.txt");
    {
        std::ofstream out(input_path, std::ios::binary);
        REQUIRE(out);
        out << input;
    }

    const auto command = runtime_command(input_path, output_path);
    const int exit_code = std::system(command.c_str());
    REQUIRE(exit_code == 0);

    std::string output;
    {
        std::ifstream in(output_path, std::ios::binary);
        REQUIRE(in);
        output.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    std::filesystem::remove(input_path);
    std::filesystem::remove(output_path);
    return aleph3::client::read_frames(output);
}

std::vector<std::string> run_runtime_allowing_failure(const std::string& input) {
    const auto input_path = test_file("bad_input.txt");
    const auto output_path = test_file("bad_output.txt");
    {
        std::ofstream out(input_path, std::ios::binary);
        REQUIRE(out);
        out << input;
    }

    const auto command = runtime_command(input_path, output_path);
    static_cast<void>(std::system(command.c_str()));

    std::string output;
    {
        std::ifstream in(output_path, std::ios::binary);
        REQUIRE(in);
        output.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    std::filesystem::remove(input_path);
    std::filesystem::remove(output_path);
    return aleph3::client::read_frames(output);
}

}  // namespace

TEST_CASE("Runtime protocol dispatches expression operations through the session", "[runtime][protocol]") {
    aleph3::tooling::RuntimeProtocolServer server;

    auto evaluate = response_json(server.handle_payload(request(1, "evaluate", {{"source", "1/2 + 1/3"}}).dump()));
    REQUIRE(evaluate["result"]["ok"] == true);
    REQUIRE(evaluate["result"]["representations"]["text/plain"] == "5/6");

    auto simplify = response_json(server.handle_payload(request(2, "simplify", {{"source", "0 + x"}}).dump()));
    REQUIRE(simplify["result"]["representations"]["text/plain"] == "x");

    auto full_form = response_json(server.handle_payload(request(3, "fullForm", {{"source", "f[x]"}}).dump()));
    REQUIRE(full_form["result"]["representations"]["text/plain"] == "f[x]");

    auto unknown = response_json(server.handle_payload(request(4, "inspect", {{"source", "x"}}).dump()));
    REQUIRE(unknown["error"]["code"] == "protocol.unknown_method");

    auto malformed_params = response_json(server.handle_payload(request(5, "evaluate", {{"query", "x"}}).dump()));
    REQUIRE(malformed_params["error"]["code"] == "protocol.invalid_params");
}

TEST_CASE("Runtime protocol exposes metadata lifecycle and reset", "[runtime][protocol]") {
    aleph3::tooling::RuntimeProtocolServer server;

    auto initialize = aleph3::client::decode_response(
        server.handle_payload(request(1, "initialize", {{"client", "tests"}, {"clientVersion", "0"}, {"protocolVersion", 1}}).dump()));
    REQUIRE(initialize.ok());
    REQUIRE(initialize.initialize.has_value());
    REQUIRE(initialize.initialize->server_name == "aleph-runtime");
    REQUIRE(initialize.initialize->capabilities.at("evaluate"));

    auto bad_initialize = response_json(
        server.handle_payload(request(2, "initialize", {{"client", "tests"}, {"clientVersion", "0"}, {"protocolVersion", 99}}).dump()));
    REQUIRE(bad_initialize["error"]["code"] == "protocol.incompatible_version");

    auto version = aleph3::client::decode_response(server.handle_payload(request(3, "version").dump()));
    REQUIRE(version.version.has_value());
    REQUIRE(version.version->protocol_version == 1);

    auto capabilities = aleph3::client::decode_response(server.handle_payload(request(4, "capabilities").dump()));
    REQUIRE(capabilities.capabilities.has_value());
    REQUIRE(capabilities.capabilities->methods.size() == 11);

    REQUIRE(response_json(server.handle_payload(request(5, "evaluate", {{"source", "a = 7"}}).dump()))["result"]["ok"] == true);
    REQUIRE(response_json(server.handle_payload(request(6, "evaluate", {{"source", "a"}}).dump()))["result"]["representations"]["text/plain"] == "7");
    auto reset = aleph3::client::decode_response(server.handle_payload(request(7, "reset").dump()));
    REQUIRE(reset.status.has_value());
    REQUIRE(reset.status->ok);
    REQUIRE(response_json(server.handle_payload(request(8, "evaluate", {{"source", "a"}}).dump()))["result"]["representations"]["text/plain"] == "a");

    auto shutdown = aleph3::client::decode_response(server.handle_payload(request(9, "shutdown").dump()));
    REQUIRE(shutdown.status.has_value());
    REQUIRE(server.shutdown_requested());
}

TEST_CASE("Runtime protocol maps discovery methods", "[runtime][protocol]") {
    aleph3::tooling::RuntimeProtocolServer server;

    auto completions = aleph3::client::decode_response(server.handle_payload(request(1, "complete", {{"query", "Fa"}}).dump()));
    REQUIRE(completions.completions.has_value());
    REQUIRE(completions.completions->size() == 1);
    REQUIRE(completions.completions->front().name == "Factor");
    REQUIRE(completions.completions->front().owning_package == "core-algebra");

    auto help = aleph3::client::decode_response(server.handle_payload(request(2, "help", {{"query", "Factor"}}).dump()));
    REQUIRE(help.help.has_value());
    REQUIRE(help.help->size() == 1);
    REQUIRE(help.help->front().name == "Factor");
    REQUIRE_FALSE(help.help->front().manual_anchor.empty());

    auto packages = aleph3::client::decode_response(server.handle_payload(request(3, "packages").dump()));
    REQUIRE(packages.packages.has_value());
    REQUIRE(packages.packages->size() == 2);
    REQUIRE(packages.packages->front().name == "core-algebra");

    REQUIRE(response_json(server.handle_payload(request(4, "evaluate", {{"source", "localName = 1"}}).dump()))["result"]["ok"] == true);
    auto local = aleph3::client::decode_response(server.handle_payload(request(5, "complete", {{"query", "local"}}).dump()));
    REQUIRE(local.completions->size() == 1);
    REQUIRE(response_json(server.handle_payload(request(6, "reset").dump()))["result"]["ok"] == true);
    auto cleared = aleph3::client::decode_response(server.handle_payload(request(7, "complete", {{"query", "local"}}).dump()));
    REQUIRE(cleared.completions->empty());
}

TEST_CASE("Aleph runtime executable handles adjacent frames and shutdown", "[runtime][process]") {
    std::string input;
    input += aleph3::client::write_frame(request(1, "initialize", {{"client", "tests"}, {"clientVersion", "0"}, {"protocolVersion", 1}}).dump());
    input += aleph3::client::write_frame(request(2, "evaluate", {{"source", "Factor[x^2 - 1]"}}).dump());
    input += aleph3::client::write_frame(request(3, "shutdown").dump());

    const auto frames = run_runtime(input);
    REQUIRE(frames.size() == 3);
    REQUIRE(response_json(frames[0])["result"]["serverName"] == "aleph-runtime");
    REQUIRE(response_json(frames[1])["result"]["representations"]["text/plain"] == "(x - 1) * (x + 1)");
    REQUIRE(response_json(frames[2])["result"]["ok"] == true);
}

TEST_CASE("Aleph runtime executable reports malformed frames deterministically", "[runtime][process]") {
    const auto frames = run_runtime_allowing_failure("Other: 2\r\n\r\n{}");
    REQUIRE(frames.size() == 1);
    const auto response = response_json(frames.front());
    REQUIRE(response["error"]["code"] == "protocol.invalid_header");
}
