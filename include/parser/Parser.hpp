// parser/Parser.hpp
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cctype>
#include <stdexcept>
#include "expr/Expr.hpp"
#include "expr/ExprUtils.hpp"

namespace mathix {

    class Parser {
    public:
        Parser(const std::string& input) : input(input), pos(0) {}

        ExprPtr parse() {
            skip_whitespace();
            size_t backup = pos;

            // Try parsing function definition
            if (std::isalpha(peek())) {
                auto name = parse_identifier();
                skip_whitespace();

                // Special case: If statement
                if (name == "If" && match('[')) {
                    pos -= 3; // Reset position to re-parse the "If[" in parse_if
                    return parse_if();
                }

                // Check for comparison operators (e.g., ==, !=, <, >, <=, >=)
                if (is_comparison_operator()) {
                    return parse_comparison(make_expr<Symbol>(name));
                }

                // Check if it's an assignment (e.g., x = 2)
                if (match('=')) {
                    auto value = parse_expression(); // Parse the assigned value
                    return make_expr<Assignment>(name, value);
                }

                // If it's followed by '[', maybe it's a definition
                if (match('[')) {
                    std::vector<std::string> args;
                    bool is_def = true;

                    size_t args_backup = pos;

                    while (true) {
                        skip_whitespace();
                        size_t id_backup = pos;
                        std::string arg;
                        try {
                            arg = parse_identifier();
                        }
                        catch (...) {
                            is_def = false;
                            break;
                        }
                        skip_whitespace();
                        if (!match('_')) {
                            is_def = false;
                            break;
                        }
                        args.push_back(arg);
                        skip_whitespace();
                        if (match(']')) break;
                        if (!match(',')) {
                            is_def = false;
                            break;
                        }
                    }

                    skip_whitespace();
                    // Handle both `:=` and `=` for function definitions
                    if (is_def && (match_string(":=") || match('='))) {
                        bool delayed = input[pos - 2] == ':'; // Check if `:=` was matched
                        auto body = parse_expression();
                        return make_expr<FunctionDefinition>(name, args, body, delayed);
                    }
                    else {
                        // Not a definition; reset and fall through to expression
                        pos = backup;
                    }
                }
                else {
                    // Not a function definition; reset
                    pos = backup;
                }
            }

            // Delegate to parse_expression for general expressions
            return parse_expression();
        }

    private:
        std::string input;
        size_t pos;

        bool is_comparison_operator() {
            skip_whitespace();

            // Check for multi-character operators first
            std::string two_chars = peek_string(2);
            if (two_chars == "==" || two_chars == "!=" || two_chars == "<=" || two_chars == ">=") {
                return true;
            }

            // Check for single-character operators
            char current = peek();
            if (current == '<' || current == '>') {
                return true;
            }

            return false;
        }

        ExprPtr parse_assignment() {
            skip_whitespace();
            auto name = parse_identifier(); // Parse the variable name
            skip_whitespace();

            if (!match('=')) {
                error("Expected '=' in assignment");
            }

            auto value = parse_expression(); // Parse the assigned value
            return make_expr<Assignment>(name, value);
        }

        ExprPtr parse_comparison(ExprPtr left) {
            skip_whitespace();

            if (match_string("==")) {
                auto right = parse_expression();
                return make_fcall("Equal", { left, right });
            }
            else if (match_string("!=")) {
                auto right = parse_expression();
                return make_fcall("NotEqual", { left, right });
            }
            else if (match_string("<=")) {
                auto right = parse_expression();
                return make_fcall("LessEqual", { left, right });
            }
            else if (match_string(">=")) {
                auto right = parse_expression();
                return make_fcall("GreaterEqual", { left, right });
            }
            else if (match('<')) {
                auto right = parse_expression();
                return make_fcall("Less", { left, right });
            }
            else if (match('>')) {
                auto right = parse_expression();
                return make_fcall("Greater", { left, right });
            }

            return left;
        }

        ExprPtr parse_expression() {
            auto left = parse_term();
            while (true) {
                skip_whitespace();
                if (match_string("<>")) {
                    auto right = parse_term(); // Parse the right-hand side
                    left = make_fcall("StringJoin", {left, right});
                }
                if (match_string("&&")) {
                    auto right = parse_term();
                    left = make_fcall("And", {left, right});
                }
                else if (match_string("||")) {
                    auto right = parse_term();
                    left = make_fcall("Or", {left, right});
                }
                else if (match('+')) {
                    auto right = parse_term();
                    left = make_fcall("Plus", {left, right});
                }
                else if (match('-')) {
                    auto right = parse_term();
                    left = make_fcall("Minus", {left, right});
                }
                else {
                    break;
                }
            }
            // Delegate to parse_comparison to handle comparison operators
            return parse_comparison(left);
        }

        ExprPtr parse_term() {
            auto left = parse_power();
            while (true) {
                skip_whitespace();
                if (match('*')) {
                    auto right = parse_power();
                    left = make_expr<FunctionCall>("Times", std::vector<ExprPtr>{left, right});
                }
                else if (match('/')) {
                    auto right = parse_power();
                    left = make_expr<FunctionCall>("Divide", std::vector<ExprPtr>{left, right});
                }
                else {
                    break;
                }
            }
            return left;
        }

        ExprPtr parse_factor() {
            skip_whitespace();

            // Handle strings
            if (match('"')) {
                size_t start = pos;
                while (pos < input.size() && input[pos] != '"') {
                    ++pos;
                }
                if (pos >= input.size() || input[pos] != '"') {
                    error("Unterminated string");
                }
                std::string value = input.substr(start, pos - start);
                ++pos; // Consume the closing quote
                return make_expr<String>(value);
            }

            // Handle unary plus/minus
            if (match('+')) {
                return parse_factor(); // Simply parse the factor after '+'
            }
            if (match('-')) {
                auto factor = parse_factor(); // Parse the factor after '-' and negate it
                return make_expr<FunctionCall>("Negate", std::vector<ExprPtr>{factor});
            }

            if (match('(')) {
                auto expr = parse_expression();
                if (!match(')')) {
                    error("Expected ')'");
                }
                return expr;
            }

            // Handle symbols (variables) or function calls
            if (std::isalpha(peek())) {
                return parse_symbol();
            }

            // Handle numbers
            if (std::isdigit(peek()) || peek() == '.') {
                auto left = parse_number();

                // If the next token is a symbol, assume it's a multiplication (e.g., "2x")
                skip_whitespace();
                if (std::isalpha(peek())) {
                    auto right = parse_symbol();
                    return make_expr<FunctionCall>("Times", std::vector<ExprPtr>{left, right});
                }

                // If the next token is '(', assume it's implicit multiplication (e.g., "2(3 + x)")
                if (peek() == '(') {
                    auto right = parse_factor(); // Parse the parenthesized expression
                    return make_expr<FunctionCall>("Times", std::vector<ExprPtr>{left, right});
                }

                return left;
            }

            // If none of the above, throw an error
            error("Expected a number, symbol, or '('");
        }

        ExprPtr parse_if() {
            skip_whitespace();
            if (!match_string("If[")) {
                error("Expected 'If['");
            }

            // Parse the condition
            auto condition = parse_expression();
            skip_whitespace();
            if (!match(',')) {
                error("Expected ',' after condition in If");
            }

            // Parse the true branch
            auto true_branch = parse_expression();
            skip_whitespace();
            if (!match(',')) {
                error("Expected ',' after true branch in If");
            }

            // Parse the false branch
            auto false_branch = parse_expression();
            skip_whitespace();
            if (!match(']')) {
                error("Expected ']' at the end of If");
            }

            // Return the parsed If expression as a FunctionCall
            return make_expr<FunctionCall>("If", std::vector<ExprPtr>{condition, true_branch, false_branch});
        }

        ExprPtr parse_symbol() {
            skip_whitespace();
            size_t start = pos;
            while (pos < input.size() && (std::isalnum(input[pos]) || input[pos] == '_')) {
                ++pos;
            }
            if (start == pos) {
                error("Expected symbol");
            }
            std::string name = input.substr(start, pos - start);

            // Handle Boolean literals
            if (name == "True") {
                return make_expr<Boolean>(true);
            }
            if (name == "False") {
                return make_expr<Boolean>(false);
            }

            skip_whitespace();
            if (match('[')) {
                // It's a function call
                std::vector<ExprPtr> args;
                if (!match(']')) { // Handle empty argument list
                    while (true) {
                        // Handle unary minus
                        if (match('-')) {
                            args.push_back(make_expr<FunctionCall>("Negate", std::vector<ExprPtr>{parse_expression()}));
                        }
                        else {
                            args.push_back(parse_expression());
                        }
                        skip_whitespace();
                        if (match(']')) {
                            break;
                        }
                        if (!match(',')) {
                            error("Expected ',' or ']' in function call");
                        }
                    }
                }
                return make_expr<FunctionCall>(name, args);
            }

            // Otherwise, it's a plain symbol (variable)
            return make_expr<Symbol>(name);
        }

        ExprPtr parse_number() {
            skip_whitespace();
            size_t start = pos;
            while (pos < input.size() && (std::isdigit(input[pos]) || input[pos] == '.')) {
                ++pos;
            }
            if (start == pos) {
                error("Expected number");
            }
            double value = std::stod(input.substr(start, pos - start));
            return make_expr<Number>(value);
        }

        ExprPtr parse_power() {
            auto left = parse_factor();
            skip_whitespace();
            while (match('^')) {
                auto right = parse_factor();
                left = make_expr<FunctionCall>("Pow", std::vector<ExprPtr>{left, right});
                skip_whitespace();
            }
            return left;
        }

        std::string parse_identifier() {
            skip_whitespace();
            size_t start = pos;
            while (pos < input.size() && std::isalnum(input[pos])) { // Only consume alphanumeric characters
                ++pos;
            }
            if (start == pos) {
                error("Expected identifier");
            }
            return input.substr(start, pos - start);
        }

        void skip_whitespace() {
            while (pos < input.size() && std::isspace(input[pos])) {
                ++pos;
            }
        }

        bool match(char expected) {
            if (pos < input.size() && input[pos] == expected) {
                ++pos;
                return true;
            }
            return false;
        }

        bool match_string(const std::string& s) {
            skip_whitespace();
            if (input.substr(pos, s.size()) == s) {
                pos += s.size();
                return true;
            }
            return false;
        }

        char peek() const {
            if (pos < input.size()) {
                return input[pos];
            }
            return '\0'; // End of input
        }

        std::string peek_string(size_t length) {
            // Ensure we don't go out of bounds
            if (pos + length > input.size()) {
                return ""; // Return an empty string if the requested length exceeds input
            }
            return input.substr(pos, length);
        }

        [[noreturn]] void error(const std::string& message) const {
            std::string pointer_line(input.size(), ' ');
            if (pos < pointer_line.size()) pointer_line[pos] = '^'; // mark where error is

            throw std::runtime_error(
                message + "\n" + input + "\n" + pointer_line
            );
        }
    };

    // Helper function to simplify usage
    inline ExprPtr parse_expression(const std::string& input) {
        Parser parser(input);
        return parser.parse();
    }

} // namespace mathix
