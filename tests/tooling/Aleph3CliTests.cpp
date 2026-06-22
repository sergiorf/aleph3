#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {

struct CommandResult {
    int exit_code = -1;
    std::string output;
};

CommandResult run_shell_command(const std::string& command) {
    std::array<char, 256> buffer{};
    std::string output;

    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        throw std::runtime_error("Failed to start command: " + command);
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    const int status = pclose(pipe);
    return {status, output};
}

}  // namespace

TEST_CASE("CLI evaluates bare expressions directly", "[tooling][cli]") {
    const auto result = run_shell_command(std::string("\"") + ALEPH3_CLI_PATH + "\" \"1+1\" 2>&1");

    REQUIRE(result.exit_code == 0);
    REQUIRE(result.output == "2\n");
}

TEST_CASE("REPL treats bare input as a default expression", "[tooling][cli]") {
    const auto command =
        std::string("printf '1+1\\n:quit\\n' | \"") + ALEPH3_CLI_PATH + "\" repl 2>&1";
    const auto result = run_shell_command(command);

    REQUIRE(result.exit_code == 0);
    REQUIRE(result.output.find("2\n") != std::string::npos);
}

TEST_CASE("REPL meta commands use colon prefixes", "[tooling][cli]") {
    const auto help_command =
        std::string("printf ':help\\n:quit\\n' | \"") + ALEPH3_CLI_PATH + "\" repl 2>&1";
    const auto help_result = run_shell_command(help_command);

    REQUIRE(help_result.exit_code == 0);
    REQUIRE(help_result.output.find("Commands") != std::string::npos);

    const auto parse_command =
        std::string("printf ':parse 1+1\\n:quit\\n' | \"") + ALEPH3_CLI_PATH + "\" repl 2>&1";
    const auto parse_result = run_shell_command(parse_command);

    REQUIRE(parse_result.exit_code == 0);
    REQUIRE(parse_result.output.find("binary_op") != std::string::npos);
}

TEST_CASE("REPL mode command reports and switches evaluators", "[tooling][cli]") {
    const auto mode_command =
        std::string("printf ':mode\\n:mode sdk\\n1+1\\n:mode symbolic\\nFactor[x^2-1]\\n:quit\\n' | \"") +
        ALEPH3_CLI_PATH + "\" repl 2>&1";
    const auto mode_result = run_shell_command(mode_command);

    REQUIRE(mode_result.exit_code == 0);
    REQUIRE(mode_result.output.find("Default mode: symbolic") != std::string::npos);
    REQUIRE(mode_result.output.find("mode set to sdk") != std::string::npos);
    REQUIRE(mode_result.output.find("2\n") != std::string::npos);
    REQUIRE(mode_result.output.find("mode set to symbolic") != std::string::npos);
    REQUIRE(mode_result.output.find("(x - 1) * (x + 1)") != std::string::npos);
}
