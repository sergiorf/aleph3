#include "aleph_client/RuntimeClient.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using aleph3::client::RuntimeClient;
using aleph3::client::RuntimeClientException;
using aleph3::client::RuntimeClientOptions;
using aleph3::client::RuntimeLookupSource;

namespace {

std::filesystem::path fake_runtime_path() {
    return std::filesystem::path(ALEPH_CLIENT_FAKE_RUNTIME_PATH);
}

std::filesystem::path real_runtime_path() {
#ifdef ALEPH_CLIENT_HAS_REAL_RUNTIME
    return std::filesystem::path(ALEPH_RUNTIME_PATH);
#else
    return {};
#endif
}

RuntimeClientOptions fake_options(std::vector<std::string> args = {}) {
    RuntimeClientOptions options;
    options.runtime_path = fake_runtime_path();
    options.runtime_arguments = std::move(args);
    options.client_name = "runtime-client-tests";
    options.startup_timeout = std::chrono::milliseconds(1000);
    options.request_timeout = std::chrono::milliseconds(1000);
    options.shutdown_timeout = std::chrono::milliseconds(1000);
    return options;
}

}  // namespace

TEST_CASE("Runtime client resolves explicit and environment runtime paths", "[aleph_client][runtime]") {
    auto explicit_options = fake_options();
    const auto explicit_lookup = aleph3::client::resolve_runtime(explicit_options);
    REQUIRE(explicit_lookup.executable_path == fake_runtime_path());
    REQUIRE(explicit_lookup.source == RuntimeLookupSource::explicit_path);

    RuntimeClientOptions env_options;
    env_options.environment["ALEPH_RUNTIME_PATH"] = fake_runtime_path().string();
    const auto env_lookup = aleph3::client::resolve_runtime(env_options);
    REQUIRE(env_lookup.executable_path == fake_runtime_path());
    REQUIRE(env_lookup.source == RuntimeLookupSource::environment);

    const auto path_runtime_name = std::string("aleph_client_path_runtime_") + fake_runtime_path().filename().string();
    const auto path_runtime = std::filesystem::temp_directory_path() / path_runtime_name;
    std::filesystem::copy_file(fake_runtime_path(), path_runtime, std::filesystem::copy_options::overwrite_existing);
    RuntimeClientOptions path_options;
    path_options.runtime_name = path_runtime_name;
    path_options.environment["PATH"] = path_runtime.parent_path().string();
    const auto path_lookup = aleph3::client::resolve_runtime(path_options);
    REQUIRE(path_lookup.executable_path == path_runtime);
    REQUIRE(path_lookup.source == RuntimeLookupSource::path);
    std::filesystem::remove(path_runtime);
}

TEST_CASE("Runtime client reports missing runtime with stable diagnostic", "[aleph_client][runtime]") {
    RuntimeClientOptions options;
    options.runtime_path = fake_runtime_path().parent_path() / "missing-runtime.exe";
    try {
        static_cast<void>(aleph3::client::resolve_runtime(options));
        FAIL("Expected missing runtime to fail");
    } catch (const RuntimeClientException& error) {
        REQUIRE(error.code() == "runtime.not_found");
    }
}

TEST_CASE("Runtime client drives fake runtime lifecycle and session state", "[aleph_client][runtime]") {
    RuntimeClient client(fake_options());
    const auto initialize = client.start();
    REQUIRE(initialize.server_name == "fake-runtime");
    REQUIRE(client.lookup_result().source == RuntimeLookupSource::explicit_path);

    auto assign = client.evaluate("a = 7");
    REQUIRE(assign.evaluation.has_value());
    REQUIRE(assign.evaluation->representations.at("text/plain") == "7");

    auto read = client.evaluate("a");
    REQUIRE(read.evaluation->representations.at("text/plain") == "7");

    auto reset = client.reset();
    REQUIRE(reset.status.has_value());
    REQUIRE(reset.status->ok);

    auto cleared = client.evaluate("a");
    REQUIRE(cleared.evaluation->representations.at("text/plain") == "a");

    REQUIRE(client.version().version.has_value());
    REQUIRE(client.capabilities().capabilities.has_value());
    REQUIRE(client.simplify("x").evaluation.has_value());
    REQUIRE(client.full_form("f[x]").evaluation.has_value());
    REQUIRE(client.help("Factor").help.has_value());
    REQUIRE(client.complete("Fa").completions.has_value());
    REQUIRE(client.packages().packages.has_value());

    client.shutdown();
    REQUIRE_FALSE(client.running());
}

TEST_CASE("Runtime client keeps protocol errors distinct from lifecycle failures", "[aleph_client][runtime]") {
    RuntimeClient client(fake_options({"protocol-error"}));
    static_cast<void>(client.start());
    const auto response = client.evaluate("x");
    REQUIRE(response.error.has_value());
    REQUIRE(response.error->code == "protocol.invalid_params");
    client.shutdown();
}

TEST_CASE("Runtime client reports incompatible initialize response", "[aleph_client][runtime]") {
    RuntimeClient client(fake_options({"incompatible"}));
    try {
        static_cast<void>(client.start());
        FAIL("Expected incompatible runtime to fail");
    } catch (const RuntimeClientException& error) {
        REQUIRE(error.code() == "runtime.incompatible_version");
        REQUIRE(error.protocol_error().has_value());
        REQUIRE(error.protocol_error()->code == "protocol.incompatible_version");
    }
}

TEST_CASE("Runtime client reports timeout, early exit, and malformed output", "[aleph_client][runtime]") {
    auto delayed = fake_options({"delay"});
    delayed.startup_timeout = std::chrono::milliseconds(25);
    try {
        RuntimeClient client(delayed);
        static_cast<void>(client.start());
        FAIL("Expected delayed runtime to time out");
    } catch (const RuntimeClientException& error) {
        REQUIRE(error.code() == "runtime.timeout");
    }

    try {
        RuntimeClient client(fake_options({"early-exit"}));
        static_cast<void>(client.start());
        FAIL("Expected early-exit runtime to fail");
    } catch (const RuntimeClientException& error) {
        REQUIRE(error.code() == "runtime.exited");
    }

    try {
        RuntimeClient client(fake_options({"malformed"}));
        static_cast<void>(client.start());
        FAIL("Expected malformed runtime output to fail");
    } catch (const RuntimeClientException& error) {
        REQUIRE(error.code() == "runtime.malformed_output");
    }
}

TEST_CASE("Runtime client launches real runtime without private headers", "[aleph_client][runtime][real]") {
#ifndef ALEPH_CLIENT_HAS_REAL_RUNTIME
    SKIP("Real runtime smoke test requires the private aleph-runtime target.");
#else
    REQUIRE(std::filesystem::exists(real_runtime_path()));
    RuntimeClientOptions options;
    options.runtime_path = real_runtime_path();
    options.client_name = "runtime-client-real-smoke";
    RuntimeClient client(options);
    const auto initialize = client.start();
    REQUIRE(initialize.server_name == "aleph-runtime");

    auto sum = client.evaluate("1/2 + 1/3");
    REQUIRE(sum.evaluation.has_value());
    REQUIRE(sum.evaluation->representations.at("text/plain") == "5/6");

    REQUIRE(client.evaluate("a = 7").evaluation.has_value());
    REQUIRE(client.evaluate("a").evaluation->representations.at("text/plain") == "7");
    REQUIRE(client.reset().status->ok);
    REQUIRE(client.evaluate("a").evaluation->representations.at("text/plain") == "a");

    client.shutdown();
    REQUIRE_FALSE(client.running());
#endif
}
