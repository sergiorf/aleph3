#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct CommandResult {
    int exit_code = -1;
    std::string output;
};

FILE* open_command_pipe(const std::string& command) {
#if defined(_WIN32)
    return _popen(command.c_str(), "r");
#else
    return popen(command.c_str(), "r");
#endif
}

int close_command_pipe(FILE* pipe) {
#if defined(_WIN32)
    return _pclose(pipe);
#else
    return pclose(pipe);
#endif
}

CommandResult run_shell_command(const std::string& command) {
    std::array<char, 256> buffer{};
    std::string output;

    FILE* pipe = open_command_pipe(command);
    if (pipe == nullptr) {
        throw std::runtime_error("Failed to start command: " + command);
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    const int status = close_command_pipe(pipe);
    output.erase(std::remove(output.begin(), output.end(), '\r'), output.end());
    return {status, output};
}

#if defined(_WIN32)
std::string escape_repl_input(std::string_view line) {
    std::string escaped;
    for (const char ch : line) {
        if (ch == '^') {
            escaped += "^^^^";
            continue;
        }
        if (ch == '&' || ch == '|' || ch == '<' || ch == '>' || ch == '(' || ch == ')') {
            escaped += "^^^";
        }
        escaped.push_back(ch);
    }
    return escaped;
}
#else
std::string escape_repl_input(std::string_view line) {
    std::string escaped;
    for (const char ch : line) {
        if (ch == '\'') {
            escaped += "'\"'\"'";
        } else {
            escaped.push_back(ch);
        }
    }
    return escaped;
}
#endif

std::string make_repl_command(const std::vector<std::string_view>& lines) {
#if defined(_WIN32)
    std::string command = "(";
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i != 0) {
            command += '&';
        }
        command += "echo " + escape_repl_input(lines[i]);
    }
    command += ") | \"" ALEPH3_CLI_PATH "\" repl 2>&1";
#else
    std::string command = "printf '%s\\n'";
    for (const auto line : lines) {
        command += " '" + escape_repl_input(line) + "'";
    }
    command += " | \"" ALEPH3_CLI_PATH "\" repl 2>&1";
#endif
    return command;
}

std::string make_direct_command(std::string_view arguments) {
#if defined(_WIN32)
    // cmd.exe requires an extra pair of enclosing quotes when the command
    // itself starts with a quoted executable path.
    return std::string("\"\"") + ALEPH3_CLI_PATH + "\" " + std::string(arguments) + " 2>&1\"";
#else
    return std::string("\"") + ALEPH3_CLI_PATH + "\" " + std::string(arguments) + " 2>&1";
#endif
}

}  // namespace

TEST_CASE("CLI evaluates bare expressions directly", "[tooling][cli]") {
    const auto command = make_direct_command("\"1+1\"");
    const auto result = run_shell_command(command);

    INFO("command: " << command);
    INFO("output: " << result.output);
    REQUIRE(result.exit_code == 0);
    REQUIRE(result.output == "2\n");
}

TEST_CASE("REPL treats bare input as a default expression", "[tooling][cli]") {
    const auto result = run_shell_command(make_repl_command({"1+1", ":quit"}));

    REQUIRE(result.exit_code == 0);
    REQUIRE(result.output.find("2\n") != std::string::npos);
}

TEST_CASE("REPL meta commands use colon prefixes", "[tooling][cli]") {
    const auto help_result = run_shell_command(make_repl_command({":help", ":quit"}));

    REQUIRE(help_result.exit_code == 0);
    REQUIRE(help_result.output.find("Commands") != std::string::npos);

    const auto parse_result = run_shell_command(make_repl_command({":parse 1+1", ":quit"}));

    REQUIRE(parse_result.exit_code == 0);
    REQUIRE(parse_result.output.find("binary_op") != std::string::npos);
}

TEST_CASE("REPL mode command reports and switches evaluators", "[tooling][cli]") {
    const auto command = make_repl_command(
        {":mode", ":mode sdk", "1+1", ":mode symbolic", "Factor[x^2-1]", ":quit"});
    const auto mode_result = run_shell_command(command);

    INFO("command: " << command);
    INFO("output: " << mode_result.output);
    REQUIRE(mode_result.exit_code == 0);
    REQUIRE(mode_result.output.find("Default mode: symbolic") != std::string::npos);
    REQUIRE(mode_result.output.find("mode set to sdk") != std::string::npos);
    REQUIRE(mode_result.output.find("2\n") != std::string::npos);
    REQUIRE(mode_result.output.find("mode set to symbolic") != std::string::npos);
    REQUIRE(mode_result.output.find("(x - 1) * (x + 1)") != std::string::npos);
}

TEST_CASE("REPL symbolic session preserves state and exposes exact rational factorization", "[tooling][cli][session]") {
    const auto result = run_shell_command(make_repl_command(
        {"a = 2", "a + 3", "Factor[(1/2)*x^2 + x + 1/2]", ":quit"}));

    REQUIRE(result.exit_code == 0);
    REQUIRE(result.output.find("5\n") != std::string::npos);
    REQUIRE(result.output.find("1/2") != std::string::npos);
    REQUIRE(result.output.find("x + 1") != std::string::npos);
}

TEST_CASE("REPL exposes session inspection and pack discovery", "[tooling][cli][session]") {
    const auto result = run_shell_command(make_repl_command(
        {":inspect f[x + 1]", ":packs", ":quit"}));

    REQUIRE(result.exit_code == 0);
    REQUIRE(result.output.find("Head: f") != std::string::npos);
    REQUIRE(result.output.find("FullForm: f[Plus[x, 1]]") != std::string::npos);
    REQUIRE(result.output.find("core-algebra:") != std::string::npos);
    REQUIRE(result.output.find("Factor") != std::string::npos);
}

TEST_CASE("REPL exposes deterministic session completion", "[tooling][cli][session][completion]") {
    const auto result = run_shell_command(make_repl_command(
        {"localValue = 2", ":complete Pol", ":complete local", ":quit"}));

    REQUIRE(result.exit_code == 0);
    REQUIRE(result.output.find("PolynomialQuotient\tpack\tcore-algebra") != std::string::npos);
    REQUIRE(result.output.find("localValue\tsymbol") != std::string::npos);
}

TEST_CASE("CLI one-shot evaluation is ephemeral and REPL recovers after errors", "[tooling][cli][session]") {
    const auto assignment = run_shell_command(make_direct_command("\"a = 2\""));
    REQUIRE(assignment.exit_code == 0);
    const auto isolated = run_shell_command(make_direct_command("\"a\""));
    REQUIRE(isolated.exit_code == 0);
    REQUIRE(isolated.output == "a\n");

    const auto repl = run_shell_command(make_repl_command({"(", "1 + 1", ":quit"}));
    REQUIRE(repl.exit_code == 0);
    REQUIRE(repl.output.find("2\n") != std::string::npos);
}
