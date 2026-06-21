#include "tooling/RewriteCliSupport.hpp"

#include <cctype>
#include <charconv>
#include <string>
#include <utility>

namespace aleph3::tooling {

namespace {

struct ParsedToken {
    std::string text;
    std::size_t start = 0;
    std::size_t end = 0;
};

std::string trim_copy(std::string_view text) {
    std::size_t start = 0;
    while (start < text.size() &&
           std::isspace(static_cast<unsigned char>(text[start])) != 0) {
        ++start;
    }

    std::size_t end = text.size();
    while (end > start &&
           std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }

    return std::string(text.substr(start, end - start));
}

bool read_next_token(std::string_view input, std::size_t& offset, ParsedToken& token, std::string& error) {
    while (offset < input.size() &&
           std::isspace(static_cast<unsigned char>(input[offset])) != 0) {
        ++offset;
    }

    if (offset >= input.size()) {
        return false;
    }

    token = {};
    token.start = offset;

    bool in_quotes = false;
    char quote = '\0';

    while (offset < input.size()) {
        const char ch = input[offset];
        if (!in_quotes && std::isspace(static_cast<unsigned char>(ch)) != 0) {
            break;
        }

        if (ch == '\\' && offset + 1 < input.size()) {
            token.text.push_back(input[offset + 1]);
            offset += 2;
            continue;
        }

        if (ch == '"' || ch == '\'') {
            if (!in_quotes) {
                in_quotes = true;
                quote = ch;
                ++offset;
                continue;
            }
            if (quote == ch) {
                in_quotes = false;
                quote = '\0';
                ++offset;
                continue;
            }
        }

        token.text.push_back(ch);
        ++offset;
    }

    if (in_quotes) {
        error = "Unterminated quoted argument in evaluate command.";
        return false;
    }

    token.end = offset;
    return true;
}

bool parse_cli_value(std::string_view text, Value& value) {
    if (text == "True") {
        value = Value(true);
        return true;
    }
    if (text == "False") {
        value = Value(false);
        return true;
    }

    double number = 0.0;
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    const auto number_result = std::from_chars(begin, end, number);
    if (number_result.ec == std::errc{} && number_result.ptr == end) {
        value = Value(number);
        return true;
    }

    value = Value(std::string(text));
    return true;
}

bool parse_binding_assignment(std::string_view assignment, std::string& name, Value& value, std::string& error) {
    const auto equal = assignment.find('=');
    if (equal == std::string_view::npos) {
        error = "Expected variable binding in the form name=value.";
        return false;
    }

    name = trim_copy(assignment.substr(0, equal));
    if (name.empty()) {
        error = "Variable binding name must not be empty.";
        return false;
    }

    const std::string raw_value = trim_copy(assignment.substr(equal + 1));
    if (raw_value.empty()) {
        error = "Variable binding value must not be empty.";
        return false;
    }

    parse_cli_value(raw_value, value);
    return true;
}

EvaluateCommandParseResult parse_evaluate_tokens(
    std::string_view command_name,
    std::span<const std::string_view> arguments,
    const std::string* raw_formula_text) {
    EvaluateCommandParseResult result;

    std::size_t index = 0;
    while (index < arguments.size()) {
        const std::string_view argument = arguments[index];
        if (argument == "--var") {
            if (index + 1 >= arguments.size()) {
                result.error_message = "Missing name=value after --var.";
                return result;
            }

            std::string name;
            Value value;
            if (!parse_binding_assignment(arguments[index + 1], name, value, result.error_message)) {
                return result;
            }

            result.options.bindings[std::move(name)] = std::move(value);
            index += 2;
            continue;
        }

        if (argument.starts_with("--var=")) {
            std::string name;
            Value value;
            if (!parse_binding_assignment(argument.substr(6), name, value, result.error_message)) {
                return result;
            }

            result.options.bindings[std::move(name)] = std::move(value);
            ++index;
            continue;
        }

        break;
    }

    if (raw_formula_text != nullptr) {
        result.options.formula = trim_copy(*raw_formula_text);
    } else {
        for (std::size_t i = index; i < arguments.size(); ++i) {
            if (!result.options.formula.empty()) {
                result.options.formula.push_back(' ');
            }
            result.options.formula.append(arguments[i]);
        }
    }

    if (result.options.formula.empty()) {
        result.error_message = "A formula is required for `" + std::string(command_name) + "`.";
    }

    return result;
}

}  // namespace

EvaluateCommandParseResult parse_formula_cli_arguments(
    std::string_view command_name,
    std::span<const std::string_view> arguments) {
    return parse_evaluate_tokens(command_name, arguments, nullptr);
}

EvaluateCommandParseResult parse_formula_repl_arguments(
    std::string_view command_name,
    std::string_view input) {
    std::vector<std::string> owned_arguments;
    std::size_t offset = 0;
    std::size_t formula_start = std::string_view::npos;
    std::string error;

    while (offset < input.size()) {
        ParsedToken token;
        if (!read_next_token(input, offset, token, error)) {
            if (!error.empty()) {
                EvaluateCommandParseResult result;
                result.error_message = std::move(error);
                return result;
            }
            break;
        }

        if (token.text == "--var" || token.text.starts_with("--var=")) {
            owned_arguments.push_back(token.text);
            if (token.text == "--var") {
                ParsedToken value_token;
                if (!read_next_token(input, offset, value_token, error)) {
                    EvaluateCommandParseResult result;
                    result.error_message = error.empty()
                        ? "Missing name=value after --var."
                        : std::move(error);
                    return result;
                }
                owned_arguments.push_back(value_token.text);
            }
            continue;
        }

        formula_start = token.start;
        break;
    }

    std::string raw_formula;
    if (formula_start != std::string_view::npos) {
        raw_formula = std::string(input.substr(formula_start));
    }

    std::vector<std::string_view> arguments;
    arguments.reserve(owned_arguments.size());
    for (const auto& argument : owned_arguments) {
        arguments.push_back(argument);
    }

    return parse_evaluate_tokens(command_name, arguments, &raw_formula);
}

EvaluateCommandParseResult parse_evaluate_cli_arguments(std::span<const std::string_view> arguments) {
    return parse_formula_cli_arguments("evaluate", arguments);
}

EvaluateCommandParseResult parse_evaluate_repl_arguments(std::string_view input) {
    return parse_formula_repl_arguments("evaluate", input);
}

ValueType infer_value_type(const Value& value) noexcept {
    if (value.is_number()) {
        return ValueType::number;
    }
    if (value.is_boolean()) {
        return ValueType::boolean;
    }
    if (value.is_string()) {
        return ValueType::string;
    }
    if (value.is_list()) {
        return ValueType::list;
    }
    return ValueType::any;
}

}  // namespace aleph3::tooling
