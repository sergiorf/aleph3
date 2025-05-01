// parser/Parser.hpp
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cctype>
#include <stdexcept>
#include "expr/Expr.hpp"

namespace mathix {

    class Parser {
    public:
        Parser(const std::string& input) : input(input), pos(0) {}

        ExprPtr parse() {
            skip_whitespace();
            // Detect function definition like f[x_] := x + 1
            if (std::isalpha(peek())) {
                size_t backup = pos; // Save the current position
                auto func_name = parse_identifier();
                skip_whitespace();
                if (match('[')) {
                    std::vector<std::string> args;
                    while (true) {
                        skip_whitespace();
                        auto arg_name = parse_identifier();
                        skip_whitespace();
                        if (!match('_')) { // Ensure `_` follows the argument name
                            pos = backup;  // Not a function definition, reset position
                            break;         // Fall through to parse_expression()
                        }
                        args.push_back(arg_name);
                        skip_whitespace();
                        if (match(']')) break;
                        if (!match(',')) error("Expected ',' or ']' in function argument list");
                    }
                    skip_whitespace();
                    if (match(':') && match('=')) { // Match `:=` for function definition
                        auto body = parse_expression();
                        return make_expr<FunctionDefinition>(func_name, args, body);
                    }
                    else {
                        pos = backup;  // Not a definition; reset position
                    }
                }
                else {
                    pos = backup;  // Not a function definition; reset position
                }
            }
            // Delegate to parse_expression() for general expressions
            return parse_expression();
        }

    private:
        std::string input;
        size_t pos;

        ExprPtr parse_expression() {
            auto left = parse_term();
            while (true) {
                skip_whitespace();
                if (match('+')) {
                    auto right = parse_term();
                    left = make_expr<FunctionCall>("Plus", std::vector<ExprPtr>{left, right});
                }
                else if (match('-')) {
                    auto right = parse_term();
                    left = make_expr<FunctionCall>("Minus", std::vector<ExprPtr>{left, right});
                }
                else {
                    break;
                }
            }
            return left;
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
                return parse_number();
            }

            // If none of the above, throw an error
            error("Expected a number, symbol, or '('");
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

            skip_whitespace();
            if (match('[')) {
                // It's a function call
                std::vector<ExprPtr> args;
                if (!match(']')) { // Handle empty argument list
                    while (true) {
                        args.push_back(parse_expression());
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

        char peek() const {
            if (pos < input.size()) {
                return input[pos];
            }
            return '\0'; // End of input
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
