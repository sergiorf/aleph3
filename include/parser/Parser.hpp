/*
 * Aleph3 Parser
 * -------------
 * This header defines the parser for Aleph3, a modern C++20-based computer algebra system.
 * The parser is responsible for converting user input (strings) into the internal Expr tree
 * representation, supporting mathematical expressions, function calls, lists, and more.
 *
 * Features:
 * - Tokenization and parsing of mathematical expressions
 * - Support for numbers, rationals, symbols, functions, and lists
 * - Error handling for invalid syntax
 *
 * See README.md for project overview.
 */
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cctype>
#include <stdexcept>
#include <cmath>
#include "expr/Expr.hpp"
#include "expr/ExprUtils.hpp"
#include "utf8.h"

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

    static bool is_letter(uint32_t cp) {
        return (cp >= 'A' && cp <= 'Z') ||
            (cp >= 'a' && cp <= 'z') ||
            (cp > 127);
    }

    static bool is_digit(uint32_t cp) {
        return (cp >= '0' && cp <= '9');
    }

    static bool is_identifier_start(uint32_t cp) {
        return cp == '_' || is_letter(cp);
    }

    static bool is_identifier_body_char(uint32_t cp) {
        return is_digit(cp) || is_letter(cp);
    }

    static bool is_symbol_body_char(uint32_t cp) {
        return cp == '_' || is_digit(cp) || is_letter(cp);
    }

    static bool is_ascii_whitespace(uint32_t cp) {
        return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == '\f' || cp == '\v';
    }

    class Parser {
        friend class ParserTestHelper;

    public:
        Parser(const std::string& input) : input(input), pos(0), paren_depth(0) {}

        ExprPtr parse() {
            skip_whitespace();
            size_t backup = pos;

            // Try parsing function definition
            if (is_identifier_start(peek())) {
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
        int paren_depth;

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
            bool pending_negate = false;

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
                if ((std::isdigit(peek()) || peek() == '.')) {
                    auto num = parse_number();
                    skip_whitespace();
                    if (peek() == '/') {
                        ++pos;
                        skip_whitespace();
                        bool denom_negative = false;
                        if (peek() == '-') {
                            denom_negative = true;
                            ++pos;
                            skip_whitespace();
                        }                        

                        skip_whitespace();
                        ExprPtr denom;
                        bool denom_is_number = false;
                        double dval = 0.0;

                        if (std::isdigit(peek()) || peek() == '.') {
                            denom = parse_number();
                            skip_whitespace();
                            if (auto* denom_n = std::get_if<Number>(&(*denom))) {
                                dval = denom_n->value;
                                denom_is_number = (std::floor(dval) == dval);
                            }
                        } else {
                            denom = parse_factor();
                            skip_whitespace();
                        }

                        // If denominator is a number, we can build a Rational
                        if (denom_is_number) {
                            if (auto* num_n = std::get_if<Number>(&(*num))) {
                                double nval = num_n->value;
                                int64_t n = -static_cast<int64_t>(nval); // unary minus on numerator
                                int64_t d = static_cast<int64_t>(dval);
                                if (denom_negative) d = -d;
                                // Normalize: if both negative, make both positive
                                if (n < 0 && d < 0) {
                                    n = -n;
                                    d = -d;
                                }
                                if (peek() == '*') {
                                    // Explicit multiplication: -2/3*x -> Times[Rational[-2,3], x]
                                    left = make_expr<Rational>(n, d);
                                    return left;
                                } else {
                                    // Implicit multiplication or end: build Rational, but do NOT return yet!
                                    if (n < 0 && d > 0) {
                                        left = make_expr<Rational>(n, d); // Keep numerator negative
                                    } else if (n > 0 && d > 0) {
                                        left = make_expr<Rational>(n, d);
                                    } else if (d < 0) {
                                        left = make_expr<Rational>(-n, -d);
                                    } else {
                                        left = make_expr<Rational>(n, d);
                                    }
                                    // Do NOT return here! Let implicit multiplication loop handle the next token.
                                }
                            }
                        } else {
                            // Not a number: treat as a full factor (e.g. -2/(3x))
                            if (auto* num_n = std::get_if<Number>(&(*num))) {
                                double nval = num_n->value;
                                left = make_expr<FunctionCall>("Divide", std::vector<ExprPtr>{
                                    make_expr<Number>(-nval), denom
                                });
                                return left;
                            }
                        }
                    }
                    skip_whitespace();
                    if (peek() == '*') {
                        if (auto* num_n = std::get_if<Number>(&(*num))) {
                            double nval = num_n->value;
                            if (std::floor(nval) == nval) {
                                left = make_expr<Number>(-nval);
                                return left;
                            }
                        }
                    }
                    // Handle implicit multiplication: -2x, -2(x), etc.
                    char next = peek();
                    if (std::isalpha(next) || next == '(') {
                        ExprPtr right = parse_factor();
                        ExprPtr lhs = left ? left : make_expr<Number>(-std::get<Number>(*num).value);
                        left = make_expr<FunctionCall>("Times", std::vector<ExprPtr>{lhs, right});
                        return left;
                    }
                    // Only here, if left is not already set, set it to a negative number
                    if (!left) {
                        left = make_expr<Number>(-std::get<Number>(*num).value);
                        return left;
                    }
                }
                // Fallback: Negate the next factor as a whole
                if (!left) {
                    ExprPtr factor = parse_factor();
                    // If factor is a Rational, fold the minus into the numerator
                    if (auto* rat = std::get_if<Rational>(&(*factor))) {
                        left = make_expr<Rational>(-rat->numerator, rat->denominator);
                    }
                    else if (auto* times = std::get_if<FunctionCall>(&(*factor))) {
                        // Handle -(Rational * x) as Times[Rational[-n, d], x]
                        if (times->head == "Times" && !times->args.empty()) {
                            if (auto* rat = std::get_if<Rational>(&(*times->args[0]))) {
                                std::vector<ExprPtr> new_args = times->args;
                                new_args[0] = make_expr<Rational>(-rat->numerator, rat->denominator);
                                left = make_expr<FunctionCall>("Times", new_args);
                            }
                            else {
                                left = make_expr<FunctionCall>("Negate", std::vector<ExprPtr>{factor});
                            }
                        }
                        else {
                            left = make_expr<FunctionCall>("Negate", std::vector<ExprPtr>{factor});
                        }
                    }
                    else if (auto* sym = std::get_if<Symbol>(&(*factor))) {
                        // -b -> Times[-1, b]
                        left = make_expr<FunctionCall>("Times", std::vector<ExprPtr>{
                            make_expr<Number>(-1), factor
                        });
                    }
                    else {
                        left = make_expr<FunctionCall>("Negate", std::vector<ExprPtr>{factor});
                    }
                }
            }
            else if (match('(')) {
                ++paren_depth;
                left = parse_expression();
                if (!match(')')) {
                    error("Expected ')'");
                }
                --paren_depth;
            }
            // Handle numbers and rationals in infix form
            else if (std::isdigit(peek()) || peek() == '.') {
                size_t backup = pos;
                left = parse_number();
                skip_whitespace();
                if (peek() == '/') {
                    ++pos; // consume '/'
                    skip_whitespace();
                    bool denom_negative = false;
                    if (peek() == '-') {
                        denom_negative = true;
                        ++pos;
                        skip_whitespace();
                    }
                    auto right = parse_number();
                    if (auto* left_num = std::get_if<Number>(&(*left))) {
                        double left_val = left_num->value;
                        if (std::floor(left_val) == left_val) {
                            if (auto* right_num = std::get_if<Number>(&(*right))) {
                                double right_val = right_num->value;
                                if (std::floor(right_val) == right_val) {
                                    int64_t n = static_cast<int64_t>(left_val);
                                    int64_t d = static_cast<int64_t>(right_val);
                                    if (denom_negative) d = -d;
                                    if (d == 0) {
                                        if (n == 0) return make_expr<Indeterminate>();
                                        return make_expr<Infinity>();
                                    }
                                    left = make_expr<Rational>(n, d);
                                    // Do not parse more as denominator! Let implicit multiplication handle next token.
                                }
                            }
                        }
                    }
                    // If not integer/integer, fallback to Divide node
                    else {
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
            }
            // Handle symbols (variables) or function calls
            else if (is_letter(peek())) {
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
                else if (name == "Complex" && match('[')) {
                    auto re_expr = parse_expression();
                    skip_whitespace();
                    if (!match(',')) error("Expected ',' in Complex");
                    auto im_expr = parse_expression();
                    skip_whitespace();
                    if (!match(']')) error("Expected ']' in Complex");
                    // Only allow number literals for now
                    auto extract_num = [](const ExprPtr& expr, double& out) -> bool {
                        if (auto* num = std::get_if<Number>(&(*expr))) {
                            out = num->value;
                            return true;
                        }
                        return false;
                    };
                    double re = 0, im = 0;
                    if (extract_num(re_expr, re) && extract_num(im_expr, im)) {
                        return make_expr<Complex>(re, im);
                    }
                    std::vector<ExprPtr> args = { re_expr, im_expr };
                    return make_expr<FunctionCall>("Complex", args);
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
                uint32_t next = peek();
                // If next token is '(', a digit, or a letter, treat as implicit multiplication
                if (next == '(' || is_digit(next) || is_letter(next)) {
                    ExprPtr right = parse_factor();
                    left = make_expr<FunctionCall>("Times", std::vector<ExprPtr>{left, right});
                }
                else {
                    break;
                }
            }

            if (pending_negate) {
                left = make_expr<FunctionCall>("Negate", std::vector<ExprPtr>{left});
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
            auto it = input.begin() + pos;
            auto end = input.end();
            std::string name;

            if (it == end) {
                error("Expected symbol");
            }
            uint32_t cp = utf8::peek_next(it, end);

            // Allow anonymous pattern symbol '_'
            if (cp == '_') {
                cp = utf8::next(it, end);
                utf8::append(cp, std::back_inserter(name));
                pos = std::distance(input.begin(), it);
            } else {
                // Otherwise, must be a valid identifier start (not '_')
                if (!is_identifier_start(cp) || cp == '_') {
                    error("Expected symbol");
                }
                cp = utf8::next(it, end);
                utf8::append(cp, std::back_inserter(name));

                // Subsequent characters: allow '_' in symbol names
                while (it != end) {
                    cp = utf8::peek_next(it, end);
                    if (!is_symbol_body_char(cp)) break;
                    cp = utf8::next(it, end);
                    utf8::append(cp, std::back_inserter(name));
                }
                pos = std::distance(input.begin(), it);
            }

            // Handle Boolean literals
            if (name == "True") {
                return make_expr<Boolean>(true);
            }
            if (name == "False") {
                return make_expr<Boolean>(false);
            }
            // Handle imaginary unit
            if (name == "I") {
                return make_expr<Complex>(0.0, 1.0);
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
            auto it = input.begin() + pos;
            auto end = input.end();
            auto start_it = it;

            while (it != end) {
                uint32_t cp = utf8::peek_next(it, end);
                if (!is_digit(cp) && cp != '.') break;
                utf8::next(it, end);
            }

            if (start_it == it) {
                error("Expected number");
            }

            std::string num_str(start_it, it);
            pos = std::distance(input.begin(), it);

            double value = std::stod(num_str);
            return make_expr<Number>(value);
        }

        std::string parse_identifier() {
            // Use iterators for UTF-8 safety
            auto it = input.begin() + pos;
            auto end = input.end();
            std::string result;

            // Skip whitespace using iterators
            while (it != end) {
                uint32_t cp = utf8::peek_next(it, end);
                if (!is_ascii_whitespace(cp)) break;
                utf8::next(it, end);
            }

            if (it == end) error("Expected identifier");
            uint32_t cp = utf8::peek_next(it, end);

            // Anonymous pattern: just '_'
            if (cp == '_') {
                cp = utf8::next(it, end);
                utf8::append(cp, std::back_inserter(result));
                pos = std::distance(input.begin(), it);
                return result;
            }

            // Otherwise, must be a valid identifier start (not '_')
            if (!is_identifier_start(cp) || cp == '_') {
                error("Expected identifier");
            }
            cp = utf8::next(it, end);
            utf8::append(cp, std::back_inserter(result));

            // Subsequent characters: identifier body (no '_')
            while (it != end) {
                cp = utf8::peek_next(it, end);
                if (!is_identifier_body_char(cp)) break;
                cp = utf8::next(it, end);
                utf8::append(cp, std::back_inserter(result));
            }

            pos = std::distance(input.begin(), it);
            return result;
        }

        void skip_whitespace() {
            auto it = input.begin() + pos;
            auto end = input.end();
            while (it != end) {
                uint32_t cp = utf8::peek_next(it, end);
                if (!is_ascii_whitespace(cp)) break;
                utf8::next(it, end);
            }
            pos = std::distance(input.begin(), it);
        }

        bool match(char expected) {
            auto it = input.begin() + pos;
            auto end = input.end();
            if (it == end) return false;
            uint32_t cp = utf8::peek_next(it, end);
            if (cp == static_cast<unsigned char>(expected)) {
                utf8::next(it, end);
                pos = std::distance(input.begin(), it);
                return true;
            }
            return false;
        }

        bool match_string(const std::string& s) {
            skip_whitespace();
            auto it = input.begin() + pos;
            auto end = input.end();
            auto sit = s.begin();
            auto send = s.end();

            auto temp_it = it;
            while (sit != send && temp_it != end) {
                uint32_t cp_input = utf8::peek_next(temp_it, end);
                uint32_t cp_s = utf8::peek_next(sit, send);
                if (cp_input != cp_s) return false;
                utf8::next(temp_it, end);
                utf8::next(sit, send);
            }
            if (sit == send) {
                pos = std::distance(input.begin(), temp_it);
                return true;
            }
            return false;
        }

        uint32_t peek() const {
            auto it = input.begin() + pos;
            auto end = input.end();
            if (it == end) return 0;
            return utf8::peek_next(it, end);
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

    inline ExprPtr try_make_complex(const ExprPtr& expr) {
        // Recognize Plus(a, Times(b, I)) and similar forms
        if (auto* call = std::get_if<FunctionCall>(&(*expr))) {
            // Handle a + b*I and a - b*I
            if (call->head == "Plus" && call->args.size() == 2) {
                double real = 0, imag = 0;
                bool real_ok = false, imag_ok = false;

                // Left as real
                if (auto* n = std::get_if<Number>(call->args[0].get())) {
                    real = n->value;
                    real_ok = true;
                }
                // Right as imaginary
                if (auto* times = std::get_if<FunctionCall>(call->args[1].get())) {
                    if (times->head == "Times" && times->args.size() == 2) {
                        // b*I
                        if (auto* n = std::get_if<Number>(times->args[0].get())) {
                            if (auto* i = std::get_if<Complex>(times->args[1].get())) {
                                if (i->real == 0.0 && i->imag == 1.0) {
                                    imag = n->value;
                                    imag_ok = true;
                                }
                            }
                        }
                        // I*b
                        if (auto* i = std::get_if<Complex>(times->args[0].get())) {
                            if (i->real == 0.0 && i->imag == 1.0) {
                                if (auto* n = std::get_if<Number>(times->args[1].get())) {
                                    imag = n->value;
                                    imag_ok = true;
                                }
                            }
                        }
                    }
                }
                // Right as real
                if (!real_ok) {
                    if (auto* n = std::get_if<Number>(call->args[1].get())) {
                        real = n->value;
                        real_ok = true;
                    }
                }
                // Left as imaginary
                if (!imag_ok) {
                    if (auto* times = std::get_if<FunctionCall>(call->args[0].get())) {
                        if (times->head == "Times" && times->args.size() == 2) {
                            // b*I
                            if (auto* n = std::get_if<Number>(times->args[0].get())) {
                                if (auto* i = std::get_if<Complex>(times->args[1].get())) {
                                    if (i->real == 0.0 && i->imag == 1.0) {
                                        imag = n->value;
                                        imag_ok = true;
                                    }
                                }
                            }
                            // I*b
                            if (auto* i = std::get_if<Complex>(times->args[0].get())) {
                                if (i->real == 0.0 && i->imag == 1.0) {
                                    if (auto* n = std::get_if<Number>(times->args[1].get())) {
                                        imag = n->value;
                                        imag_ok = true;
                                    }
                                }
                            }
                        }
                    }
                }
                if (real_ok && imag_ok) {
                    return make_expr<Complex>(real, imag);
                }
            }
            // Handle pure imaginary: Times(Number, I) or Times(I, Number)
            if (call->head == "Times" && call->args.size() == 2) {
                if (auto* n = std::get_if<Number>(call->args[0].get())) {
                    if (auto* i = std::get_if<Complex>(call->args[1].get())) {
                        if (i->real == 0.0 && i->imag == 1.0) {
                            return make_expr<Complex>(0.0, n->value);
                        }
                    }
                }
                if (auto* i = std::get_if<Complex>(call->args[0].get())) {
                    if (i->real == 0.0 && i->imag == 1.0) {
                        if (auto* n = std::get_if<Number>(call->args[1].get())) {
                            return make_expr<Complex>(0.0, n->value);
                        }
                    }
                }
            }
        }
        // Just I
        if (auto* i = std::get_if<Complex>(&(*expr))) {
            if (i->real == 0.0 && i->imag == 1.0) {
                return expr;
            }
        }
        return expr;
    }

    // Helper function to simplify usage
    inline ExprPtr parse_expression(const std::string& input) {
        Parser parser(input);
        auto expr = parser.parse();
        return try_make_complex(expr);
    }

}
