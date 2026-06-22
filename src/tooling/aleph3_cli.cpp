#include "frontend/Lexer.hpp"
#include "frontend/Parser.hpp"
#include "ir/Node.hpp"
#include "sdk/Engine.hpp"
#include "tooling/DemoHostFunctions.hpp"
#include "tooling/RewriteCliSupport.hpp"
#if defined(ALEPH3_HAS_SYMBOLIC_ENGINE)
#include "tooling/SymbolicCliSupport.hpp"
#endif

#include <cctype>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <algorithm>
#include <vector>

#if !defined(_WIN32)
#include <termios.h>
#include <unistd.h>
#endif

namespace {

enum class ReplMode {
    sdk,
    symbolic
};

using aleph3::Diagnostic;
using aleph3::SourceSpan;
using aleph3::frontend::Token;
using aleph3::frontend::TokenKind;

struct CliPalette {
    bool color_stdout = false;
    bool color_stderr = false;
    std::string accent;
    std::string accent_soft;
    std::string highlight;
    std::string success;
    std::string warning;
    std::string error;
    std::string dim;
    std::string reset;
};

const CliPalette& cli_palette() {
    static const CliPalette palette = [] {
        CliPalette value;
#if !defined(_WIN32)
        value.color_stdout = ::isatty(STDOUT_FILENO) != 0;
        value.color_stderr = ::isatty(STDERR_FILENO) != 0;
#endif
        if (value.color_stdout || value.color_stderr) {
            value.accent = "\033[38;5;45m";
            value.accent_soft = "\033[38;5;81m";
            value.highlight = "\033[1;38;5;223m";
            value.success = "\033[38;5;84m";
            value.warning = "\033[38;5;215m";
            value.error = "\033[38;5;203m";
            value.dim = "\033[38;5;244m";
            value.reset = "\033[0m";
        }
        return value;
    }();
    return palette;
}

std::string style_stdout(std::string_view text, const std::string& color) {
    const auto& palette = cli_palette();
    if (!palette.color_stdout || color.empty()) {
        return std::string(text);
    }
    return color + std::string(text) + palette.reset;
}

std::string style_stderr(std::string_view text, const std::string& color) {
    const auto& palette = cli_palette();
    if (!palette.color_stderr || color.empty()) {
        return std::string(text);
    }
    return color + std::string(text) + palette.reset;
}

void print_cli_logo(bool compact = false) {
    const auto& palette = cli_palette();
    const auto title = style_stdout("ℵ3 aleph3", palette.highlight);

    if (compact) {
        std::cout << title << ' ' << style_stdout("symbolic sdk + math core", palette.dim) << "\n\n";
        return;
    }

    std::cout
        << "              " << style_stdout("ℵ", palette.accent) << '\n'
        << "           " << style_stdout("aleph-three", palette.accent_soft) << '\n'
        << "  " << title << "  " << style_stdout("symbolic sdk + embeddable math engine", palette.dim) << "\n\n";
}

std::string span_to_string(const SourceSpan& span) {
    std::ostringstream out;
    out << "line " << span.line << ", col " << span.column
        << " [" << span.start_offset << ", " << span.end_offset << ")";
    return out.str();
}

void print_diagnostic(const Diagnostic& diagnostic) {
    std::cerr << diagnostic.code << ": " << diagnostic.message;
    if (!diagnostic.span.empty()) {
        std::cerr << " at " << span_to_string(diagnostic.span);
    }
    std::cerr << '\n';
}

void print_token(const Token& token) {
    std::cout << aleph3::frontend::to_string(token.kind)
              << " `" << token.lexeme << "`"
              << " @ " << span_to_string(token.span);

    if (const auto* number = token.as<double>()) {
        std::cout << " value=" << *number;
    } else if (const auto* boolean = token.as<bool>()) {
        std::cout << " value=" << (*boolean ? "True" : "False");
    } else if (const auto* string = token.as<std::string>()) {
        std::cout << " value=\"" << *string << "\"";
    }

    std::cout << '\n';
}

const char* node_kind_name(aleph3::ir::NodeKind kind) {
    switch (kind) {
        case aleph3::ir::NodeKind::number_literal:
            return "number_literal";
        case aleph3::ir::NodeKind::boolean_literal:
            return "boolean_literal";
        case aleph3::ir::NodeKind::string_literal:
            return "string_literal";
        case aleph3::ir::NodeKind::variable:
            return "variable";
        case aleph3::ir::NodeKind::unary_op:
            return "unary_op";
        case aleph3::ir::NodeKind::binary_op:
            return "binary_op";
        case aleph3::ir::NodeKind::call:
            return "call";
        case aleph3::ir::NodeKind::if_expr:
            return "if_expr";
    }

    return "unknown";
}

void print_indent(int depth) {
    for (int i = 0; i < depth; ++i) {
        std::cout << "  ";
    }
}

void print_ir(const aleph3::ir::NodePtr& node, int depth = 0) {
    if (node == nullptr) {
        print_indent(depth);
        std::cout << "<null>\n";
        return;
    }

    print_indent(depth);
    std::cout << node_kind_name(node->kind) << '\n';

    if (const auto* number = node->as<aleph3::ir::NumberLiteralNode>()) {
        print_indent(depth + 1);
        std::cout << "value: " << number->value << '\n';
        return;
    }
    if (const auto* boolean = node->as<aleph3::ir::BooleanLiteralNode>()) {
        print_indent(depth + 1);
        std::cout << "value: " << (boolean->value ? "True" : "False") << '\n';
        return;
    }
    if (const auto* string = node->as<aleph3::ir::StringLiteralNode>()) {
        print_indent(depth + 1);
        std::cout << "value: \"" << string->value << "\"\n";
        return;
    }
    if (const auto* variable = node->as<aleph3::ir::VariableNode>()) {
        print_indent(depth + 1);
        std::cout << "name: " << variable->name << '\n';
        return;
    }
    if (const auto* unary = node->as<aleph3::ir::UnaryOpNode>()) {
        print_indent(depth + 1);
        std::cout << "op: " << (unary->op == aleph3::ir::UnaryOperator::plus ? "plus" : "minus") << '\n';
        print_ir(unary->operand, depth + 1);
        return;
    }
    if (const auto* binary = node->as<aleph3::ir::BinaryOpNode>()) {
        print_indent(depth + 1);
        std::cout << "lhs:\n";
        print_ir(binary->left, depth + 2);
        print_indent(depth + 1);
        std::cout << "rhs:\n";
        print_ir(binary->right, depth + 2);
        return;
    }
    if (const auto* call = node->as<aleph3::ir::CallNode>()) {
        print_indent(depth + 1);
        std::cout << "callee: " << call->callee << '\n';
        for (const auto& argument : call->arguments) {
            print_ir(argument, depth + 1);
        }
        return;
    }
    if (const auto* if_node = node->as<aleph3::ir::IfNode>()) {
        print_indent(depth + 1);
        std::cout << "condition:\n";
        print_ir(if_node->condition, depth + 2);
        print_indent(depth + 1);
        std::cout << "then:\n";
        print_ir(if_node->then_branch, depth + 2);
        print_indent(depth + 1);
        std::cout << "else:\n";
        print_ir(if_node->else_branch, depth + 2);
    }
}

void print_usage() {
    print_cli_logo(true);
    std::cout
        << style_stdout("Usage", cli_palette().accent) << '\n'
        << "  aleph3_cli help\n"
        << "  aleph3_cli examples\n"
        << "  aleph3_cli repl\n"
        << "  aleph3_cli host-functions\n"
        << "  aleph3_cli tokens <formula>\n"
        << "  aleph3_cli parse <formula>\n"
        << "  aleph3_cli validate <formula>\n"
        << "  aleph3_cli compile <formula>\n"
        << "  aleph3_cli evaluate [--var name=value]... <formula>\n"
        << "  aleph3_cli evaluate-host [--var name=value]... <formula>\n";
#if defined(ALEPH3_HAS_SYMBOLIC_ENGINE)
    std::cout
        << "  aleph3_cli symbolic-evaluate <expr>\n"
        << "  aleph3_cli symbolic-simplify <expr>\n"
        << "  aleph3_cli symbolic-fullform <expr>\n";
#endif
    std::cout << '\n'
              << style_stdout("Tip", cli_palette().highlight) << ": run `aleph3_cli help` for the full command guide.\n";
}

std::string join_formula_args(int argc, char** argv, int start_index) {
    std::ostringstream out;
    for (int i = start_index; i < argc; ++i) {
        if (i > start_index) {
            out << ' ';
        }
        out << argv[i];
    }
    return out.str();
}

void print_help() {
    print_cli_logo();
    std::cout
        << style_stdout("Commands", cli_palette().accent) << '\n'
        << "  help                 Show this help text\n"
        << "  examples             Show example formulas and commands\n"
        << "  repl                 Start an interactive prompt\n"
        << "  host-functions       List the demo registered host functions\n"
        << "  tokens <formula>     Lex a formula and print the token stream\n"
        << "  parse <formula>      Parse a formula and print the SDK IR tree\n"
        << "  validate <formula>   Run lexer -> parser -> schema/policy validation\n"
        << "  compile <formula>    Build a reusable compiled formula handle\n"
        << "  evaluate [--var name=value]... <formula>\n"
        << "                       Compile and evaluate a formula with CLI bindings\n"
        << "  evaluate-host [--var name=value]... <formula>\n"
        << "                       Evaluate using demo registered host functions\n"
#if defined(ALEPH3_HAS_SYMBOLIC_ENGINE)
        << "  symbolic-evaluate <expr>\n"
        << "                       Evaluate through the symbolic engine surface\n"
        << "  symbolic-simplify <expr>\n"
        << "                       Evaluate and simplify through the symbolic engine\n"
        << "  symbolic-fullform <expr>\n"
        << "                       Print the symbolic expression in FullForm\n"
#endif
        << "\n"
        << style_stdout("Notes", cli_palette().accent) << '\n'
        << "  validate is live, but with the default empty schema it will reject\n"
        << "  unknown variables and functions.\n"
        << "  evaluate automatically allows variables passed through --var.\n"
        << "  Binding values support numbers, True, False, and quoted/unquoted strings.\n"
        << "  evaluate-host registers the demo host bundle shown by host-functions.\n"
#if defined(ALEPH3_HAS_SYMBOLIC_ENGINE)
        << "  symbolic-* commands use the broader symbolic engine, including\n"
        << "  polynomial functions such as Expand, Factor, Collect, GCD, and PolynomialQuotient.\n"
        << "  Factor currently supports common-content extraction and integer-coefficient\n"
        << "  univariate rational-root factorization in the supported subset.\n"
#endif
        << "  In the REPL, bare input evaluates as an expression.\n"
        << "  Prefix shell/meta commands with `:` such as `:help`, `:parse`, or `:quit`.\n"
        << "  Use `:mode` to inspect the active evaluator, or `:mode symbolic` / `:mode sdk`\n"
        << "  to switch the bare-expression backend.\n"
        << "  In the REPL on Unix-like terminals, up/down arrows walk command history,\n"
        << "  left/right arrows move the cursor, and Tab completes command names.\n"
        << "\n"
        << style_stdout("Quick Start", cli_palette().accent) << '\n'
        << "  aleph3_cli repl\n"
        << "  aleph3_cli symbolic-evaluate \"Factor[x^2 - 1]\"\n"
        << "  aleph3_cli evaluate-host --var x=12 \"Clamp[x, 0, 10]\"\n";
}

void print_host_functions() {
    print_cli_logo(true);
    std::cout
        << style_stdout("Demo Host Functions", cli_palette().accent) << '\n'
        << "These are registered by `evaluate-host` and by the SDK example.\n"
        << '\n';

    for (const auto& function : aleph3::tooling::demo_host_function_docs()) {
        std::cout
            << function.signature << '\n'
            << "  " << function.description << '\n'
            << "  CLI:  " << function.cli_example << '\n'
            << "  REPL: " << function.repl_example << '\n'
            << '\n';
    }
}

void print_examples() {
    print_cli_logo(true);
    std::cout
        << style_stdout("Examples", cli_palette().accent) << '\n'
        << "\n"
        << style_stdout("CLI overview", cli_palette().highlight) << '\n'
        << "  aleph3_cli help\n"
        << "  aleph3_cli host-functions\n"
        << "  aleph3_cli tokens \"If[x >= 1, \\\"ok\\\", False]\"\n"
        << "  aleph3_cli parse \"2 + 3 * (x + 1)\"\n"
        << "  aleph3_cli validate \"1 + 2\"\n"
        << "  aleph3_cli validate \"If[True, 1, \\\"no\\\"]\"\n"
        << "  aleph3_cli compile \"1 + 2\"\n"
        << "  aleph3_cli evaluate --var x=3 \"x + 1\"\n"
        << "  aleph3_cli evaluate --var label=hello \"label + 1\"\n"
        << "  aleph3_cli evaluate \"If[3 < 4, 10, 20]\"\n"
#if defined(ALEPH3_HAS_SYMBOLIC_ENGINE)
        << "  aleph3_cli symbolic-evaluate \"Expand[(x + 1) * (x + 2)]\"\n"
        << "  aleph3_cli symbolic-simplify \"0 + (1 * x)\"\n"
        << "  aleph3_cli symbolic-evaluate \"PolynomialQuotient[x^2 - 1, x - 1, x]\"\n"
#endif
        << "\n"
        << style_stdout("Host function examples", cli_palette().highlight) << '\n';

    for (const auto& function : aleph3::tooling::demo_host_function_docs()) {
        std::cout << "  " << function.cli_example << '\n';
    }

    std::cout
        << "\n"
        << style_stdout("REPL examples", cli_palette().highlight) << '\n'
        << "  > 1 + 1\n"
        << "  > Factor[x^2 - 1]\n"
        << "  > :mode\n"
        << "  > :mode sdk\n"
        << "  > :help\n"
        << "  > :examples\n"
        << "  > :host-functions\n"
        << "  > :tokens If[x >= 1, \\\"ok\\\", False]\n"
        << "  > :parse 2 + 3 * (x + 1)\n"
        << "  > :validate 1 + 2\n"
        << "  > :compile 1 + 2\n"
        << "  > :evaluate --var x=3 x + 1\n"
        << "  > :evaluate-host --var x=12 Clamp[x, 0, 10]\n"
        << "  > :quit\n";
}

const std::vector<std::string>& repl_commands() {
    static const std::vector<std::string> commands = {
        ":help",
        ":examples",
        ":host-functions",
        ":mode",
        ":tokens",
        ":parse",
        ":validate",
        ":compile",
        ":evaluate",
        ":evaluate-host",
#if defined(ALEPH3_HAS_SYMBOLIC_ENGINE)
        ":symbolic-evaluate",
        ":symbolic-simplify",
        ":symbolic-fullform",
#endif
        ":quit",
        ":exit"
    };
    return commands;
}

bool is_known_repl_command(std::string_view command) {
    const auto& commands = repl_commands();
    return std::find(commands.begin(), commands.end(), command) != commands.end();
}

bool is_known_cli_command(std::string_view command) {
    return command == "help" || command == "--help" || command == "-h" ||
           command == "examples" || command == "host-functions" || command == "repl" ||
           command == "tokens" || command == "parse" || command == "validate" ||
           command == "compile" || command == "evaluate" || command == "evaluate-host"
#if defined(ALEPH3_HAS_SYMBOLIC_ENGINE)
           || command == "symbolic-evaluate" || command == "symbolic-simplify" ||
              command == "symbolic-fullform"
#endif
        ;
}

std::string_view strip_repl_command_prefix(std::string_view command) {
    if (!command.empty() && command.front() == ':') {
        command.remove_prefix(1);
    }
    return command;
}

ReplMode default_repl_mode() {
#if defined(ALEPH3_HAS_SYMBOLIC_ENGINE)
    return ReplMode::symbolic;
#else
    return ReplMode::sdk;
#endif
}

std::string_view repl_mode_name(ReplMode mode) {
    switch (mode) {
        case ReplMode::sdk:
            return "sdk";
        case ReplMode::symbolic:
            return "symbolic";
    }
    return "unknown";
}

std::string_view repl_mode_short_name(ReplMode mode) {
    switch (mode) {
        case ReplMode::sdk:
            return "sdk";
        case ReplMode::symbolic:
            return "sym";
    }
    return "?";
}

std::string make_repl_prompt(ReplMode mode) {
    return style_stdout("ℵ3", cli_palette().highlight) +
           style_stdout("[", cli_palette().dim) +
           style_stdout(repl_mode_short_name(mode), cli_palette().accent_soft) +
           style_stdout("]", cli_palette().dim) +
           style_stdout(" >", cli_palette().accent) + " ";
}

std::string trim(std::string text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

int run_evaluate_command(const aleph3::tooling::EvaluateCommandOptions& options) {
    aleph3::Engine engine;
    aleph3::Schema schema;

    for (const auto& [name, value] : options.bindings) {
        schema.allow_variable({name, aleph3::tooling::infer_value_type(value), true});
    }

    const auto compile_result = engine.compile(options.formula, schema);
    for (const auto& diagnostic : compile_result.diagnostics) {
        print_diagnostic(diagnostic);
    }
    if (!compile_result.ok()) {
        return 2;
    }

    const auto evaluation_result = engine.evaluate(*compile_result.formula, options.bindings);
    if (!evaluation_result.ok()) {
        if (evaluation_result.error.has_value()) {
            std::cerr << evaluation_result.error->code << ": " << evaluation_result.error->message;
            if (evaluation_result.error->span.has_value()) {
                std::cerr << " at " << span_to_string(*evaluation_result.error->span);
            }
            std::cerr << '\n';
        }
        return 2;
    }

    if (const auto* number = evaluation_result.value->as_number()) {
        std::cout << *number << '\n';
    } else if (const auto* boolean = evaluation_result.value->as_boolean()) {
        std::cout << (*boolean ? "True" : "False") << '\n';
    } else if (const auto* string = evaluation_result.value->as_string()) {
        std::cout << *string << '\n';
    } else {
        std::cout << "<value>\n";
    }
    return 0;
}

int run_host_evaluate_command(const aleph3::tooling::EvaluateCommandOptions& options) {
    aleph3::Engine engine;
    aleph3::Schema schema;
    aleph3::tooling::register_demo_host_functions(engine, schema);

    for (const auto& [name, value] : options.bindings) {
        schema.allow_variable({name, aleph3::tooling::infer_value_type(value), true});
    }

    const auto compile_result = engine.compile(options.formula, schema);
    for (const auto& diagnostic : compile_result.diagnostics) {
        print_diagnostic(diagnostic);
    }
    if (!compile_result.ok()) {
        return 2;
    }

    const auto evaluation_result = engine.evaluate(*compile_result.formula, options.bindings);
    if (!evaluation_result.ok()) {
        if (evaluation_result.error.has_value()) {
            std::cerr << evaluation_result.error->code << ": " << evaluation_result.error->message;
            if (evaluation_result.error->span.has_value()) {
                std::cerr << " at " << span_to_string(*evaluation_result.error->span);
            }
            std::cerr << '\n';
        }
        return 2;
    }

    if (const auto* number = evaluation_result.value->as_number()) {
        std::cout << *number << '\n';
    } else if (const auto* boolean = evaluation_result.value->as_boolean()) {
        std::cout << (*boolean ? "True" : "False") << '\n';
    } else if (const auto* string = evaluation_result.value->as_string()) {
        std::cout << *string << '\n';
    } else {
        std::cout << "<value>\n";
    }
    return 0;
}

#if !defined(_WIN32)
class RawTerminalMode {
public:
    RawTerminalMode() {
        active_ = ::isatty(STDIN_FILENO) != 0;
        if (!active_) {
            return;
        }

        if (::tcgetattr(STDIN_FILENO, &original_) != 0) {
            active_ = false;
            return;
        }

        termios raw = original_;
        raw.c_lflag &= static_cast<unsigned long>(~(ICANON | ECHO));
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        if (::tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
            active_ = false;
        }
    }

    RawTerminalMode(const RawTerminalMode&) = delete;
    RawTerminalMode& operator=(const RawTerminalMode&) = delete;

    ~RawTerminalMode() {
        if (active_) {
            ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_);
        }
    }

    [[nodiscard]] bool active() const noexcept {
        return active_;
    }

private:
    bool active_ = false;
    termios original_{};
};
#endif

void redraw_prompt_line(const std::string& prompt, const std::string& line, std::size_t cursor) {
    std::cout << '\r' << prompt << line << "\033[K";
    const std::size_t tail = line.size() > cursor ? line.size() - cursor : 0;
    if (tail > 0) {
        std::cout << "\033[" << tail << 'D';
    }
    std::cout << std::flush;
}

std::vector<std::string> complete_command_prefix(const std::string& prefix) {
    std::vector<std::string> matches;
    for (const auto& command : repl_commands()) {
        if (command.starts_with(prefix)) {
            matches.push_back(command);
        }
    }
    return matches;
}

std::string common_prefix(const std::vector<std::string>& values) {
    if (values.empty()) {
        return "";
    }

    std::string prefix = values.front();
    for (std::size_t i = 1; i < values.size(); ++i) {
        std::size_t j = 0;
        while (j < prefix.size() && j < values[i].size() && prefix[j] == values[i][j]) {
            ++j;
        }
        prefix.resize(j);
        if (prefix.empty()) {
            break;
        }
    }
    return prefix;
}

void print_completion_choices(const std::vector<std::string>& matches) {
    std::cout << '\n';
    for (const auto& match : matches) {
        std::cout << style_stdout("  " + match, cli_palette().accent_soft) << '\n';
    }
}

bool try_complete_repl_line(const std::string& prompt, std::string& buffer, std::size_t& cursor) {
    const auto split = buffer.find_first_of(" \t");
    if (split != std::string::npos && cursor > split) {
        return false;
    }

    const std::string prefix = buffer.substr(0, cursor);
    const auto matches = complete_command_prefix(prefix);
    if (matches.empty()) {
        return false;
    }

    if (matches.size() == 1) {
        buffer = matches.front();
        cursor = buffer.size();
        if (buffer.find(' ') == std::string::npos) {
            buffer.push_back(' ');
            ++cursor;
        }
        redraw_prompt_line(prompt, buffer, cursor);
        return true;
    }

    const std::string prefix_match = common_prefix(matches);
    if (prefix_match.size() > prefix.size()) {
        buffer.replace(0, cursor, prefix_match);
        cursor = prefix_match.size();
        redraw_prompt_line(prompt, buffer, cursor);
        return true;
    }

    print_completion_choices(matches);
    redraw_prompt_line(prompt, buffer, cursor);
    return true;
}

bool read_repl_line(const std::string& prompt, std::vector<std::string>& history, std::string& line) {
#if defined(_WIN32)
    std::cout << prompt;
    return static_cast<bool>(std::getline(std::cin, line));
#else
    if (::isatty(STDIN_FILENO) == 0) {
        std::cout << prompt;
        return static_cast<bool>(std::getline(std::cin, line));
    }

    RawTerminalMode raw_mode;
    if (!raw_mode.active()) {
        std::cout << prompt;
        return static_cast<bool>(std::getline(std::cin, line));
    }

    std::string buffer;
    std::size_t cursor = 0;
    std::size_t history_index = history.size();
    std::cout << prompt << std::flush;

    while (true) {
        char ch = '\0';
        const auto bytes_read = ::read(STDIN_FILENO, &ch, 1);
        if (bytes_read <= 0) {
            std::cout << '\n';
            return false;
        }

        if (ch == '\n' || ch == '\r') {
            std::cout << '\n';
            line = buffer;
            return true;
        }

        if (ch == 4) {
            if (buffer.empty()) {
                std::cout << '\n';
                return false;
            }
            continue;
        }

        if (ch == 127 || ch == '\b') {
            if (cursor > 0) {
                buffer.erase(cursor - 1, 1);
                --cursor;
                redraw_prompt_line(prompt, buffer, cursor);
            }
            continue;
        }

        if (ch == '\t') {
            try_complete_repl_line(prompt, buffer, cursor);
            continue;
        }

        if (ch == '\033') {
            char sequence[2] = {'\0', '\0'};
            if (::read(STDIN_FILENO, &sequence[0], 1) <= 0 ||
                ::read(STDIN_FILENO, &sequence[1], 1) <= 0) {
                continue;
            }

            if (sequence[0] == '[' && sequence[1] == 'A') {
                if (!history.empty() && history_index > 0) {
                    --history_index;
                    buffer = history[history_index];
                    cursor = buffer.size();
                    redraw_prompt_line(prompt, buffer, cursor);
                }
                continue;
            }

            if (sequence[0] == '[' && sequence[1] == 'B') {
                if (history_index < history.size()) {
                    ++history_index;
                    buffer = history_index < history.size() ? history[history_index] : "";
                    cursor = buffer.size();
                    redraw_prompt_line(prompt, buffer, cursor);
                }
                continue;
            }

            if (sequence[0] == '[' && sequence[1] == 'C') {
                if (cursor < buffer.size()) {
                    ++cursor;
                    redraw_prompt_line(prompt, buffer, cursor);
                }
                continue;
            }

            if (sequence[0] == '[' && sequence[1] == 'D') {
                if (cursor > 0) {
                    --cursor;
                    redraw_prompt_line(prompt, buffer, cursor);
                }
                continue;
            }

            continue;
        }

        if (std::isprint(static_cast<unsigned char>(ch)) != 0) {
            buffer.insert(buffer.begin() + static_cast<std::ptrdiff_t>(cursor), ch);
            ++cursor;
            redraw_prompt_line(prompt, buffer, cursor);
        }
    }
#endif
}

int run_default_expression(std::string_view input, ReplMode mode = default_repl_mode()) {
    if (mode == ReplMode::sdk) {
        const auto evaluate_options = aleph3::tooling::parse_evaluate_repl_arguments(input);
        if (!evaluate_options.ok()) {
            std::cerr << style_stderr(evaluate_options.error_message, cli_palette().error) << '\n';
            return 2;
        }
        return run_evaluate_command(evaluate_options.options);
    }

#if defined(ALEPH3_HAS_SYMBOLIC_ENGINE)
    const auto result = aleph3::tooling::symbolic_evaluate_expression(input);
    if (!result.ok) {
        std::cerr << style_stderr(result.error_message, cli_palette().error) << '\n';
        return 2;
    }
    std::cout << result.output << '\n';
    return 0;
#else
    std::cerr << style_stderr("Symbolic mode is not available in this build.", cli_palette().error) << '\n';
    return 2;
#endif
}

int run_command(std::string_view command, std::string_view formula) {
#if defined(ALEPH3_HAS_SYMBOLIC_ENGINE)
    if (command == "symbolic-evaluate") {
        const auto result = aleph3::tooling::symbolic_evaluate_expression(formula);
        if (!result.ok) {
            std::cerr << style_stderr(result.error_message, cli_palette().error) << '\n';
            return 2;
        }
        std::cout << result.output << '\n';
        return 0;
    }

    if (command == "symbolic-simplify") {
        const auto result = aleph3::tooling::symbolic_simplify_expression(formula);
        if (!result.ok) {
            std::cerr << style_stderr(result.error_message, cli_palette().error) << '\n';
            return 2;
        }
        std::cout << result.output << '\n';
        return 0;
    }

    if (command == "symbolic-fullform") {
        const auto result = aleph3::tooling::symbolic_fullform_expression(formula);
        if (!result.ok) {
            std::cerr << style_stderr(result.error_message, cli_palette().error) << '\n';
            return 2;
        }
        std::cout << result.output << '\n';
        return 0;
    }
#endif

    if (command == "tokens") {
        aleph3::frontend::Lexer lexer(formula);
        const auto result = lexer.tokenize();
        for (const auto& token : result.tokens) {
            print_token(token);
        }
        for (const auto& diagnostic : result.diagnostics) {
            print_diagnostic(diagnostic);
        }
        return result.ok() ? 0 : 2;
    }

    if (command == "parse") {
        aleph3::frontend::Parser parser(formula);
        const auto result = parser.parse();
        if (!result.ok()) {
            for (const auto& diagnostic : result.diagnostics) {
                print_diagnostic(diagnostic);
            }
            return 2;
        }

        print_ir(result.root);
        return 0;
    }

    if (command == "host-functions") {
        print_host_functions();
        return 0;
    }

    aleph3::Engine engine;
    aleph3::Schema schema;

    if (command == "validate") {
        const auto result = engine.validate(formula, schema);
        for (const auto& diagnostic : result.diagnostics) {
            print_diagnostic(diagnostic);
        }
        if (result.ok) {
            std::cout << style_stdout("validation ok", cli_palette().success) << '\n';
        }
        return result.ok ? 0 : 2;
    }

    if (command == "compile") {
        const auto result = engine.compile(formula, schema);
        for (const auto& diagnostic : result.diagnostics) {
            print_diagnostic(diagnostic);
        }
        if (result.ok()) {
            std::cout << style_stdout("compile ok", cli_palette().success) << '\n';
        }
        return result.ok() ? 0 : 2;
    }

    if (command == "evaluate") {
        const auto evaluate_options = aleph3::tooling::parse_evaluate_repl_arguments(formula);
        if (!evaluate_options.ok()) {
            std::cerr << style_stderr(evaluate_options.error_message, cli_palette().error) << '\n';
            return 2;
        }
        return run_evaluate_command(evaluate_options.options);
    }

    if (command == "evaluate-host") {
        const auto evaluate_options = aleph3::tooling::parse_formula_repl_arguments("evaluate-host", formula);
        if (!evaluate_options.ok()) {
            std::cerr << style_stderr(evaluate_options.error_message, cli_palette().error) << '\n';
            return 2;
        }
        return run_host_evaluate_command(evaluate_options.options);
    }

    return run_default_expression(command);
}

int run_repl() {
    print_cli_logo();
    ReplMode repl_mode = default_repl_mode();
    std::cout
        << style_stdout("Interactive REPL", cli_palette().accent) << '\n'
        << "Type expressions directly. Use `:help` for commands and `:quit` to exit.\n"
        << "Use arrows for history/editing and Tab for command completion.\n"
        << "Default mode: " << repl_mode_name(repl_mode) << "\n\n";

    std::string line;
    std::vector<std::string> history;
    while (true) {
        const std::string prompt = make_repl_prompt(repl_mode);
        if (!read_repl_line(prompt, history, line)) {
            std::cout << '\n';
            return 0;
        }

        line = trim(std::move(line));
        if (line.empty()) {
            continue;
        }

        history.push_back(line);

        if (line.empty() || line.front() != ':') {
            const int exit_code = run_default_expression(line, repl_mode);
            if (exit_code != 0) {
                std::cerr << style_stderr("(expression failed with exit code " + std::to_string(exit_code) + ")",
                                          cli_palette().warning)
                          << '\n';
            }
            continue;
        }

        const auto split = line.find_first_of(" \t");
        const std::string command = split == std::string::npos ? line : line.substr(0, split);
        const std::string formula = split == std::string::npos ? "" : trim(line.substr(split + 1));

        if (!is_known_repl_command(command)) {
            std::cerr << style_stderr("Unknown REPL command", cli_palette().error) << ": " << command << '\n';
            continue;
        }

        const std::string normalized_command(strip_repl_command_prefix(command));

        if (normalized_command == "quit" || normalized_command == "exit") {
            return 0;
        }
        if (normalized_command == "help") {
            print_help();
            continue;
        }
        if (normalized_command == "mode") {
            if (formula.empty()) {
                std::cout << repl_mode_name(repl_mode) << '\n';
                continue;
            }

            if (formula == "sdk") {
                repl_mode = ReplMode::sdk;
                std::cout << style_stdout("mode set to sdk", cli_palette().success) << '\n';
                continue;
            }
#if defined(ALEPH3_HAS_SYMBOLIC_ENGINE)
            if (formula == "symbolic") {
                repl_mode = ReplMode::symbolic;
                std::cout << style_stdout("mode set to symbolic", cli_palette().success) << '\n';
                continue;
            }
#endif
            std::cerr << style_stderr("Unknown mode", cli_palette().error) << ": " << formula << '\n';
            continue;
        }
        if (normalized_command == "examples") {
            print_examples();
            continue;
        }
        if (normalized_command == "host-functions") {
            print_host_functions();
            continue;
        }

        if ((normalized_command == "tokens" || normalized_command == "parse" || normalized_command == "validate" ||
             normalized_command == "compile" || normalized_command == "evaluate" ||
             normalized_command == "evaluate-host"
#if defined(ALEPH3_HAS_SYMBOLIC_ENGINE)
             || normalized_command == "symbolic-evaluate" || normalized_command == "symbolic-simplify" ||
                normalized_command == "symbolic-fullform"
#endif
             ) &&
            formula.empty()) {
            std::cerr << style_stderr("A formula is required", cli_palette().warning)
                      << " for `" << command << "`.\n";
            continue;
        }

        const int exit_code = run_command(normalized_command, formula);
        if (exit_code != 0) {
            std::cerr << style_stderr("(command failed with exit code " + std::to_string(exit_code) + ")",
                                      cli_palette().warning)
                      << '\n';
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        return run_repl();
    }

    const std::string_view command = argv[1];

    if (!is_known_cli_command(command)) {
        return run_default_expression(join_formula_args(argc, argv, 1));
    }

    if (command == "help" || command == "--help" || command == "-h") {
        print_help();
        return 0;
    }
    if (command == "examples") {
        print_examples();
        return 0;
    }
    if (command == "host-functions") {
        print_host_functions();
        return 0;
    }
    if (command == "repl") {
        return run_repl();
    }

    if (command == "evaluate") {
        std::vector<std::string_view> arguments;
        arguments.reserve(static_cast<std::size_t>(argc - 2));
        for (int i = 2; i < argc; ++i) {
            arguments.emplace_back(argv[i]);
        }

        const auto evaluate_options = aleph3::tooling::parse_evaluate_cli_arguments(arguments);
        if (!evaluate_options.ok()) {
            std::cerr << evaluate_options.error_message << '\n';
            return 2;
        }
        return run_evaluate_command(evaluate_options.options);
    }

    if (command == "evaluate-host") {
        std::vector<std::string_view> arguments;
        arguments.reserve(static_cast<std::size_t>(argc - 2));
        for (int i = 2; i < argc; ++i) {
            arguments.emplace_back(argv[i]);
        }

        const auto evaluate_options = aleph3::tooling::parse_formula_cli_arguments("evaluate-host", arguments);
        if (!evaluate_options.ok()) {
            std::cerr << evaluate_options.error_message << '\n';
            return 2;
        }
        return run_host_evaluate_command(evaluate_options.options);
    }

    if (argc < 3) {
        print_usage();
        return 1;
    }

    return run_command(command, join_formula_args(argc, argv, 2));
}
