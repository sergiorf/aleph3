#include "frontend/Lexer.hpp"
#include "frontend/Parser.hpp"
#include "ir/Node.hpp"
#include "sdk/Engine.hpp"

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

using aleph3::Diagnostic;
using aleph3::SourceSpan;
using aleph3::frontend::Token;
using aleph3::frontend::TokenKind;

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
    std::cout
        << "aleph3 rewrite CLI\n"
        << "usage:\n"
        << "  aleph3_rewrite_cli help\n"
        << "  aleph3_rewrite_cli examples\n"
        << "  aleph3_rewrite_cli repl\n"
        << "  aleph3_rewrite_cli tokens <formula>\n"
        << "  aleph3_rewrite_cli parse <formula>\n"
        << "  aleph3_rewrite_cli validate <formula>\n"
        << "  aleph3_rewrite_cli compile <formula>\n";
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
    std::cout
        << "Aleph3 rewrite CLI help\n"
        << "\n"
        << "Commands:\n"
        << "  help                 Show this help text\n"
        << "  examples             Show example formulas and commands\n"
        << "  repl                 Start an interactive prompt\n"
        << "  tokens <formula>     Lex a formula and print the token stream\n"
        << "  parse <formula>      Parse a formula and print the rewrite IR tree\n"
        << "  validate <formula>   Run lexer -> parser -> schema/policy validation\n"
        << "  compile <formula>    Build a reusable compiled formula handle\n"
        << "  evaluate <formula>   Compile and evaluate a formula with empty bindings\n"
        << "\n"
        << "Notes:\n"
        << "  validate is live, but with the default empty schema it will reject\n"
        << "  unknown variables and functions.\n"
        << "  evaluate currently uses empty bindings in the CLI, so formulas with\n"
        << "  variables still need SDK-level tests or future CLI binding support.\n"
        << "  In the REPL on Unix-like terminals, up/down arrows walk command history,\n"
        << "  left/right arrows move the cursor, and Tab completes command names.\n";
}

void print_examples() {
    std::cout
        << "Examples\n"
        << "\n"
        << "  aleph3_rewrite_cli tokens \"If[x >= 1, \\\"ok\\\", False]\"\n"
        << "  aleph3_rewrite_cli parse \"2 + 3 * (x + 1)\"\n"
        << "  aleph3_rewrite_cli validate \"1 + 2\"\n"
        << "  aleph3_rewrite_cli compile \"1 + 2\"\n"
        << "  aleph3_rewrite_cli evaluate \"If[3 < 4, 10, 20]\"\n"
        << "\n"
        << "REPL examples\n"
        << "  > help\n"
        << "  > examples\n"
        << "  > tokens If[x >= 1, \\\"ok\\\", False]\n"
        << "  > parse 2 + 3 * (x + 1)\n"
        << "  > validate 1 + 2\n"
        << "  > compile 1 + 2\n"
        << "  > evaluate If[3 < 4, 10, 20]\n"
        << "  > quit\n";
}

const std::vector<std::string>& repl_commands() {
    static const std::vector<std::string> commands = {
        "help",
        "examples",
        "tokens",
        "parse",
        "validate",
        "compile",
        "evaluate",
        "quit",
        "exit"
    };
    return commands;
}

std::string trim(std::string text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
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
        std::cout << match << '\n';
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

int run_command(std::string_view command, std::string_view formula) {
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

    aleph3::Engine engine;
    aleph3::Schema schema;

    if (command == "validate") {
        const auto result = engine.validate(formula, schema);
        for (const auto& diagnostic : result.diagnostics) {
            print_diagnostic(diagnostic);
        }
        if (result.ok) {
            std::cout << "validation ok\n";
        }
        return result.ok ? 0 : 2;
    }

    if (command == "compile") {
        const auto result = engine.compile(formula, schema);
        for (const auto& diagnostic : result.diagnostics) {
            print_diagnostic(diagnostic);
        }
        if (result.ok()) {
            std::cout << "compile ok\n";
        }
        return result.ok() ? 0 : 2;
    }

    if (command == "evaluate") {
        const auto compile_result = engine.compile(formula, schema);
        for (const auto& diagnostic : compile_result.diagnostics) {
            print_diagnostic(diagnostic);
        }
        if (!compile_result.ok()) {
            return 2;
        }

        const auto evaluation_result = engine.evaluate(*compile_result.formula, {});
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

    std::cerr << "Unknown command: " << command << '\n';
    return 1;
}

int run_repl() {
    std::cout << "Aleph3 rewrite REPL\n";
    std::cout << "Type `help` for commands, `quit` to exit. Use arrows for history/editing and Tab for command completion.\n";

    std::string line;
    std::vector<std::string> history;
    constexpr std::string_view prompt = "> ";
    while (true) {
        if (!read_repl_line(std::string(prompt), history, line)) {
            std::cout << '\n';
            return 0;
        }

        line = trim(std::move(line));
        if (line.empty()) {
            continue;
        }

        history.push_back(line);

        if (line == "quit" || line == "exit") {
            return 0;
        }
        if (line == "help") {
            print_help();
            continue;
        }
        if (line == "examples") {
            print_examples();
            continue;
        }

        const auto split = line.find_first_of(" \t");
        const std::string command = split == std::string::npos ? line : line.substr(0, split);
        const std::string formula = split == std::string::npos ? "" : trim(line.substr(split + 1));

        if ((command == "tokens" || command == "parse" || command == "validate" || command == "compile" || command == "evaluate") &&
            formula.empty()) {
            std::cerr << "A formula is required for `" << command << "`.\n";
            continue;
        }

        const int exit_code = run_command(command, formula);
        if (exit_code != 0) {
            std::cerr << "(command failed with exit code " << exit_code << ")\n";
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        return run_repl();
    }

    const std::string_view command = argv[1];

    if (command == "help" || command == "--help" || command == "-h") {
        print_help();
        return 0;
    }
    if (command == "examples") {
        print_examples();
        return 0;
    }
    if (command == "repl") {
        return run_repl();
    }

    if (argc < 3) {
        print_usage();
        return 1;
    }

    return run_command(command, join_formula_args(argc, argv, 2));
}
