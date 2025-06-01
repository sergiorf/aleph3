// parser/Parser.hpp
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cctype>
#include <stdexcept>
#include "expr/Expr.hpp"
#include "expr/ExprUtils.hpp"

namespace aleph3 {

    enum class Assoc { Left, Right };

    struct OperatorInfo {
        int precedence;
        Assoc assoc;
        std::string ast_name; // "Plus", "Times", "Rule", etc.
    };

    // Operator table: precedence (higher = tighter), associativity, AST head
    static const std::vector<std::pair<std::string, OperatorInfo>> infix_operators = {
        {"->",   {1, Assoc::Right, "Rule"}},
        {"==",   {2, Assoc::Left,  "Equal"}},
        {"!=",   {2, Assoc::Left,  "NotEqual"}},
        {"<=",   {2, Assoc::Left,  "LessEqual"}},
        {">=",   {2, Assoc::Left,  "GreaterEqual"}},
        {"<",    {2, Assoc::Left,  "Less"}},
        {">",    {2, Assoc::Left,  "Greater"}},
        {"||",   {3, Assoc::Left,  "Or"}},
        {"&&",   {4, Assoc::Left,  "And"}},
        {"<>",   {5, Assoc::Left,  "StringJoin"}},
        {"+",    {6, Assoc::Left,  "Plus"}},
        {"-",    {6, Assoc::Left,  "Minus"}},
        {"*",    {7, Assoc::Left,  "Times"}},
        {"/",    {7, Assoc::Left,  "Divide"}},
        {"^",    {8, Assoc::Right, "Power"}},
    };

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

                // Check if it's an assignment (e.g., x = 2)
                if (peek_string(2) != "==" && match('=')) {
                    auto value = parse_expression(); // Parse the assigned value
                    return make_expr<Assignment>(name, value);
                }

                // If it's followed by '[', maybe it's a definition
                if (match('[')) {
                    std::vector<Parameter> params;
                    bool is_def = true;

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
                        skip_whitespace();
                        ExprPtr default_value = nullptr;
                        if (match(':')) {
                            // Parse the default value expression
                            default_value = parse_expression();
                        }
                        params.emplace_back(arg, default_value);
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
                        return make_expr<FunctionDefinition>(name, params, body, delayed);
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

            // Delegate to Pratt parser for general expressions
            return parse_expression();
        }

    private:
        std::string input;
        size_t pos;

        // Pratt/precedence climbing parser
        ExprPtr parse_expression(int min_precedence = 1) {
            auto left = parse_factor();

            while (true) {
                skip_whitespace();
                std::string op = peek_operator();
                if (op.empty()) break;

                OperatorInfo info = get_operator_info(op);
                if (info.precedence < min_precedence) break;

                pos += op.size(); // consume operator

                int next_min_prec = info.assoc == Assoc::Left ? info.precedence + 1 : info.precedence;
                auto right = parse_expression(next_min_prec);

                if (info.ast_name == "Rule") {
                    left = make_expr<Rule>(left, right);
                }
                else if (info.ast_name == "StringJoin") {
                    // Flatten left if it's also a StringJoin
                    std::vector<ExprPtr> args;
                    if (auto* left_call = std::get_if<FunctionCall>(&(*left));
                        left_call && left_call->head == "StringJoin") {
                        args = left_call->args;
                    }
                    else {
                        args.push_back(left);
                    }
                    // Flatten right if it's also a StringJoin (rare, but for completeness)
                    if (auto* right_call = std::get_if<FunctionCall>(&(*right));
                        right_call && right_call->head == "StringJoin") {
                        args.insert(args.end(), right_call->args.begin(), right_call->args.end());
                    }
                    else {
                        args.push_back(right);
                    }
                    left = make_fcall("StringJoin", args);
                }
                else {
                    left = make_fcall(info.ast_name, { left, right });
                }
            }
            return left;
        }

        ExprPtr parse_factor() {
            skip_whitespace();

            ExprPtr left;

            // Handle lists: { ... }
            if (match('{')) {
                std::vector<ExprPtr> elements;
                skip_whitespace();
                if (!match('}')) { // Non-empty list
                    while (true) {
                        elements.push_back(parse_expression());
                        skip_whitespace();
                        if (match('}')) break;
                        if (!match(',')) {
                            error("Expected ',' or '}' in list");
                        }
                    }
                }
                left = make_expr<FunctionCall>("List", elements);
            }
            // Handle strings
            else if (match('"')) {
                size_t start = pos;
                while (pos < input.size() && input[pos] != '"') {
                    ++pos;
                }
                if (pos >= input.size() || input[pos] != '"') {
                    error("Unterminated string");
                }
                std::string value = input.substr(start, pos - start);
                ++pos; // Consume the closing quote
                left = make_expr<String>(value);
            }
            // Handle unary plus/minus
            else if (match('+')) {
                left = parse_factor(); // Simply parse the factor after '+'
            }
            else if (match('-')) {
                skip_whitespace();
                size_t backup = pos;
                ExprPtr factor;

                // Try to parse a number or rational
                if (std::isdigit(peek()) || peek() == '.') {
                    factor = parse_number();
                    skip_whitespace();
                    if (peek() == '/') {
                        ++pos; // consume '/'
                        skip_whitespace();
                        auto right = parse_number();
                        if (auto* left_num = std::get_if<Number>(&(*factor))) {
                            double left_val = left_num->value;
                            if (std::floor(left_val) == left_val) {
                                if (auto* right_num = std::get_if<Number>(&(*right))) {
                                    double right_val = right_num->value;
                                    if (std::floor(right_val) == right_val) {
                                        int64_t n = static_cast<int64_t>(left_val);
                                        int64_t d = static_cast<int64_t>(right_val);
                                        if (d == 0) {
                                            if (n == 0) return make_expr<Indeterminate>();
                                            return make_expr<Infinity>();
                                        }
                                        return factor = make_expr<Rational>(n, d);
                                    }
                                }
                            }
                        }
                        // If not integer/integer, fallback to Divide node
                        pos = backup;
                        auto left_expr = parse_number();
                        skip_whitespace();
                        if (peek() == '/') {
                            ++pos;
                            auto right_expr = parse_factor();
                            factor = make_fcall("Divide", { left_expr, right_expr });
                        }
                    }
                } else {
                    factor = parse_factor();
                }

                // Check for implicit multiplication after the negative factor
                skip_whitespace();
                char next = peek();
                if (next == '(' || std::isalpha(next)) {
                    ExprPtr right = parse_factor();
                    ExprPtr times = make_expr<FunctionCall>("Times", std::vector<ExprPtr>{factor, right});
                    return make_expr<FunctionCall>("Negate", std::vector<ExprPtr>{times});
                } else {
                    return make_expr<FunctionCall>("Negate", std::vector<ExprPtr>{factor});
                }
            }
            else if (match('(')) {
                left = parse_expression();
                if (!match(')')) {
                    error("Expected ')'");
                }
            }
            // Handle numbers and rationals in infix form
            else if (std::isdigit(peek()) || peek() == '.') {
                size_t backup = pos;
                left = parse_number();
                skip_whitespace();
                if (peek() == '/') {
                    ++pos; // consume '/'
                    skip_whitespace();
                    if (auto* left_num = std::get_if<Number>(&(*left))) {
                        double left_val = left_num->value;
                        if (std::floor(left_val) == left_val) {
                            auto right = parse_number();
                            if (auto* right_num = std::get_if<Number>(&(*right))) {
                                double right_val = right_num->value;
                                if (std::floor(right_val) == right_val) {
                                    int64_t n = static_cast<int64_t>(left_val);
                                    int64_t d = static_cast<int64_t>(right_val);
                                    if (d == 0) {
                                        if (n == 0) return make_expr<Indeterminate>();
                                        return make_expr<Infinity>();
                                    }
                                    return make_expr<Rational>(n, d);
                                }
                            }
                        }
                    }
                    // If not integer/integer, fallback to Divide node
                    pos = backup;
                    auto left_expr = parse_number();
                    skip_whitespace();
                    if (peek() == '/') {
                        ++pos;
                        auto right_expr = parse_factor();
                        left = make_fcall("Divide", { left_expr, right_expr });
                    }
                }
            }
            // Handle symbols (variables) or function calls
            else if (std::isalpha(peek())) {
                size_t id_start = pos;
                std::string name = parse_identifier();
                skip_whitespace();
                if (name == "Rational" && match('[')) {
                    auto num_expr = parse_expression();
                    skip_whitespace();
                    if (!match(',')) error("Expected ',' in Rational");
                    auto den_expr = parse_expression();
                    skip_whitespace();
                    if (!match(']')) error("Expected ']' in Rational");
                    // Only allow integer literals
                    auto extract_int = [](const ExprPtr& expr, int64_t& out) -> bool {
                        if (auto* num = std::get_if<Number>(&(*expr))) {
                            double val = num->value;
                            if (std::floor(val) == val) {
                                out = static_cast<int64_t>(val);
                                return true;
                            }
                        }
                        if (auto* neg = std::get_if<FunctionCall>(&(*expr))) {
                            if (neg->head == "Negate" && neg->args.size() == 1) {
                                if (auto* num = std::get_if<Number>(&(*neg->args[0]))) {
                                    double val = num->value;
                                    if (std::floor(val) == val) {
                                        out = -static_cast<int64_t>(val);
                                        return true;
                                    }
                                }
                            }
                        }
                        return false;
                        };

                    int64_t n, d;
                    if (extract_int(num_expr, n) && extract_int(den_expr, d)) {
                        if (d == 0) {
                            if (n == 0) return make_expr<Indeterminate>();
                            return make_expr<Infinity>();
                        }
                        return make_expr<Rational>(n, d);
                    }
                    std::vector<ExprPtr> args = { num_expr, den_expr };
                    left = make_expr<FunctionCall>("Rational", args);
                }
                else {
                    pos = id_start;
                    left = parse_symbol();
                }
            }
            else {
                error("Expected a number, symbol, or '('");
            }

            // --- Implicit multiplication loop ---
            while (true) {
                skip_whitespace();
                char next = peek();
                // If next token is '(', a digit, or a letter, treat as implicit multiplication
                if (next == '(' || std::isdigit(next) || std::isalpha(next)) {
                    ExprPtr right = parse_factor();
                    left = make_expr<FunctionCall>("Times", std::vector<ExprPtr>{left, right});
                }
                else {
                    break;
                }
            }
            return left;
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

        // --- Operator helpers ---
        OperatorInfo get_operator_info(const std::string& op) {
            for (const auto& pair : infix_operators) {
                if (op == pair.first) return pair.second;
            }
            return { -1, Assoc::Left, "" }; // Not found
        }

        std::string peek_operator() {
            skip_whitespace();
            // Try to match the longest operator first
            size_t max_len = 0;
            std::string matched_op;
            for (const auto& pair : infix_operators) {
                const std::string& op = pair.first;
                if (input.substr(pos, op.size()) == op && op.size() > max_len) {
                    matched_op = op;
                    max_len = op.size();
                }
            }
            return matched_op;
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

}
