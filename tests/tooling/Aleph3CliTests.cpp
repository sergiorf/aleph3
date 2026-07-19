#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
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

std::size_t count_substrings(std::string_view text, std::string_view needle) {
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string_view::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

class TemporaryScript {
public:
    TemporaryScript(std::string name, std::string contents)
        : path_(std::filesystem::temp_directory_path() / std::move(name)) {
        std::ofstream stream(path_, std::ios::binary | std::ios::trunc);
        stream << contents;
    }
    ~TemporaryScript() { std::error_code ignored; std::filesystem::remove(path_, ignored); }
    [[nodiscard]] const std::filesystem::path& path() const { return path_; }
private:
    std::filesystem::path path_;
};

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
    REQUIRE(help_result.output.find(":reset") != std::string::npos);
    REQUIRE(help_result.output.find("Discovery") != std::string::npos);
    REQUIRE(help_result.output.find("Builtins") != std::string::npos);
    REQUIRE(help_result.output.find("Discovered packs") != std::string::npos);
    REQUIRE(help_result.output.find("User-defined") != std::string::npos);

    const auto parse_result = run_shell_command(make_repl_command({":parse 1+1", ":quit"}));

    REQUIRE(parse_result.exit_code == 0);
    REQUIRE(parse_result.output.find("binary_op") != std::string::npos);
}

TEST_CASE("REPL help exposes focused symbolic and command entries", "[tooling][cli][session][help]") {
    const auto result = run_shell_command(make_repl_command(
        {":help Factor", ":help Clear", ":help :reset", ":help core-algebra", ":help Log",
         ":help ArcTan", ":help Length", ":help FullForm", ":help Assuming", ":help MatrixAdd", ":help D", ":quit"}));

    REQUIRE(result.exit_code == 0);
    REQUIRE(result.output.find("Factor [pack] (core-algebra)") != std::string::npos);
    REQUIRE(result.output.find("Factor[x^2 - 1] -> (x - 1) * (x + 1)") != std::string::npos);
    REQUIRE(result.output.find("Manual: manual/packs-algebra.md#factor") != std::string::npos);
    REQUIRE(result.output.find("Clear [builtin]") != std::string::npos);
    REQUIRE(result.output.find("Clear cannot remove builtin") != std::string::npos);
    REQUIRE(result.output.find(":reset") != std::string::npos);
    REQUIRE(result.output.find("without unloading builtins or packs") != std::string::npos);
    REQUIRE(result.output.find("PolynomialQuotient [pack] (core-algebra)") != std::string::npos);
    REQUIRE(result.output.find("Log [builtin]") != std::string::npos);
    REQUIRE(result.output.find("Log[base, x]") != std::string::npos);
    REQUIRE(result.output.find("ArcTan [builtin]") != std::string::npos);
    REQUIRE(result.output.find("ArcTan[x, y]") != std::string::npos);
    REQUIRE(result.output.find("Length [builtin]") != std::string::npos);
    REQUIRE(result.output.find("Length[{a, b, c}] -> 3") != std::string::npos);
    REQUIRE(result.output.find("FullForm [builtin]") != std::string::npos);
    REQUIRE(result.output.find("FullForm[x + 1] -> \"Plus[x, 1]\"") != std::string::npos);
    REQUIRE(result.output.find("Assuming [builtin]") != std::string::npos);
    REQUIRE(result.output.find("Facts are scoped to the evaluation") != std::string::npos);
    REQUIRE(result.output.find("MatrixAdd [pack] (core-algebra)") != std::string::npos);
    REQUIRE(result.output.find("MatrixAdd[a, b]") != std::string::npos);
    REQUIRE(result.output.find("D [pack] (core-calculus)") != std::string::npos);
    REQUIRE(result.output.find("Manual: manual/packs-calculus.md#differentiation") != std::string::npos);
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

TEST_CASE("CLI evaluates exact matrix pack expressions", "[tooling][cli][matrix]") {
    const auto expression = std::string(1, char(34)) + "Det[{{1,2},{3,4}}]" + char(34);
    const auto result = run_shell_command(make_direct_command(expression));
    REQUIRE(result.exit_code == 0);
    REQUIRE(result.output == "-2\n");
}

TEST_CASE("CLI evaluates focused calculus pack expressions", "[tooling][cli][calculus]") {
    const auto expression = std::string(1, char(34)) + "D[x^2+3*x,x]" + char(34);
    const auto result = run_shell_command(make_direct_command(expression));
    REQUIRE(result.exit_code == 0);
    REQUIRE(result.output == "2 * x + 3\n");
}

TEST_CASE("CLI evaluates variable dependency inspection expressions", "[tooling][cli][variables]") {
    const auto expression = std::string(1, char(34)) + "DependsOn[f[a_]->g[a,y],a]" + char(34);
    const auto result = run_shell_command(make_direct_command(expression));
    REQUIRE(result.exit_code == 0);
    REQUIRE(result.output == "False\n");
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
    REQUIRE(result.output.find("core-calculus:") != std::string::npos);
    REQUIRE(result.output.find("Differentiate") != std::string::npos);
}

TEST_CASE("REPL exposes deterministic session completion", "[tooling][cli][session][completion]") {
    const auto result = run_shell_command(make_repl_command(
        {"localValue = 2", ":complete Pol", ":complete Full", ":complete local", ":quit"}));

    REQUIRE(result.exit_code == 0);
    REQUIRE(result.output.find("PolynomialQuotient\tpack\tcore-algebra") != std::string::npos);
    REQUIRE(result.output.find("FullForm\tbuiltin") != std::string::npos);
    REQUIRE(result.output.find("localValue\tsymbol") != std::string::npos);
}

TEST_CASE("REPL reset discards symbolic session state", "[tooling][cli][session]") {
    const auto result = run_shell_command(make_repl_command(
        {"a = 2", "a", ":reset", "a", ":quit"}));

    REQUIRE(result.exit_code == 0);
    REQUIRE(result.output.find("2\n") != std::string::npos);
    REQUIRE(result.output.find("session reset") != std::string::npos);
    REQUIRE(result.output.find("a\n") != std::string::npos);
}

TEST_CASE("REPL reset removes session-local completions", "[tooling][cli][session][completion]") {
    const auto result = run_shell_command(make_repl_command(
        {"localResetSymbol = 1", ":complete localReset", ":reset", ":complete localReset", ":quit"}));

    REQUIRE(result.exit_code == 0);
    REQUIRE(result.output.find("localResetSymbol\tsymbol") != std::string::npos);
    REQUIRE(count_substrings(result.output, "localResetSymbol\tsymbol") == 1);
}

TEST_CASE("REPL completion reflects provider precedence and cleanup", "[tooling][cli][session][completion]") {
    const auto result = run_shell_command(make_repl_command(
        {"localHelpName = 1", "Plus[x_, y_] := 99", ":complete localHelp",
         ":complete Plus", "Clear[localHelpName]", ":complete localHelp", ":quit"}));

    REQUIRE(result.exit_code == 0);
    REQUIRE(result.output.find("localHelpName\tsymbol") != std::string::npos);
    REQUIRE(count_substrings(result.output, "localHelpName\tsymbol") == 1);
    REQUIRE(result.output.find("Plus\tbuiltin") != std::string::npos);
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

TEST_CASE("CLI scripts preserve state and continue after failures", "[tooling][cli][script]") {
    TemporaryScript script(
        "aleph3-script-state.txt",
        "a = 2\n\na + 3\nRefine[x, And[x > 0, x <= 0]]\n1 + 1\n");
    const auto result = run_shell_command(make_direct_command(
        "script \"" + script.path().string() + "\""));

    REQUIRE(result.exit_code == 2);
    REQUIRE(result.output.find("5\n") != std::string::npos);
    REQUIRE(result.output.find("line 4: runtime.assumption_contradiction") != std::string::npos);
    REQUIRE(result.output.find("2\n") != std::string::npos);
}

TEST_CASE("CLI scripts emit deterministic JSON Lines", "[tooling][cli][script][json]") {
    TemporaryScript script(
        "aleph3-script-json.txt",
        "Refine[x, And[x > 0, x <= 0]]\n1 + 1\n");
    const auto result = run_shell_command(make_direct_command(
        "script --json \"" + script.path().string() + "\""));

    REQUIRE(result.exit_code == 2);
    REQUIRE(result.output.find("\"schema_version\":1") != std::string::npos);
    REQUIRE(result.output.find("\"line\":1") != std::string::npos);
    REQUIRE(result.output.find("\"code\":\"runtime.assumption_contradiction\"") != std::string::npos);
    REQUIRE(result.output.find("\"line\":2") != std::string::npos);
    REQUIRE(result.output.find("\"output\":\"2\"") != std::string::npos);
    REQUIRE(result.output.find("Aleph3") == std::string::npos);
}

TEST_CASE("CLI scripts enforce usage and line limits", "[tooling][cli][script][limits]") {
    const auto usage = run_shell_command(make_direct_command("script"));
    REQUIRE(usage.exit_code == 2);
    REQUIRE(usage.output.find("Usage:") != std::string::npos);

    TemporaryScript script(
        "aleph3-script-oversized-line.txt",
        std::string(1024u * 1024u + 1u, 'x'));
    const auto oversized = run_shell_command(make_direct_command(
        "script \"" + script.path().string() + "\""));
    REQUIRE(oversized.exit_code == 3);
    REQUIRE(oversized.output.find("1 MiB limit") != std::string::npos);
}
