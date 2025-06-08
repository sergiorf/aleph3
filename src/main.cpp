#include "expr/Expr.hpp"
#include "expr/FullForm.hpp"
#include "evaluator/Evaluator.hpp"
#include "evaluator/EvaluationContext.hpp"
#include "evaluator/BuiltInFunctions.hpp"
#include "parser/Parser.hpp"
#include "help/HelpTexts.hpp"

#include <iostream>
#include <string>
#include <map>
#include <algorithm>
#include <locale>
#ifdef _WIN32
#include <windows.h>
#endif

using namespace aleph3;

// Colors for CLI output
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_CAT     "\033[36m"   // Cyan for categories
#define COLOR_FUNC    "\033[33m"   // Yellow for function names
#define COLOR_DESC    "\033[37m"   // White/gray for descriptions
#define COLOR_PROMPT  "\033[32m"   // Green for input prompt
#define COLOR_OUT     "\033[36m"   // Cyan for output
#define COLOR_NUM     "\033[33m"   // Yellow for numbers
#define COLOR_ERR     "\033[31m"   // Red for errors

template <typename Container>
std::string join(const Container& container, const std::string& delimiter) {
    std::string result;
    auto it = container.begin();
    if (it != container.end()) {
        result += *it;
        ++it;
    }
    while (it != container.end()) {
        result += delimiter + *it;
        ++it;
    }
    return result;
}

void show_paginated(const std::vector<std::string>& lines, size_t page_size = 20) {
    size_t count = 0;
    for (const auto& line : lines) {
        std::cout << line << std::endl;
        if (++count % page_size == 0 && count < lines.size()) {
            std::cout << "-- More -- (press Enter to continue, q to quit) ";
            std::string dummy;
            std::getline(std::cin, dummy);
            if (!dummy.empty() && (dummy[0] == 'q' || dummy[0] == 'Q')) break;
        }
    }
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    std::locale::global(std::locale("en_US.UTF-8"));
    EvaluationContext ctx;
    int counter = 1;

    aleph3::register_built_in_functions();

    std::cout << COLOR_BOLD << "Welcome to Aleph3 CLI!" << COLOR_RESET << std::endl;
    std::cout << "Type 'exit' to quit." << std::endl;

    while (true) {
        std::cout << COLOR_PROMPT << "In[" << counter << "]:= " << COLOR_RESET;
        std::string input;
        if (!std::getline(std::cin, input)) break;
        if (input == "exit") break;
        if (input.empty()) continue;

        if (input == "?" || input == "help") {
            std::map<std::string, std::vector<std::pair<std::string, std::string>>> categories;
            for (const auto& entry : aleph3::get_help_entries()) {
                categories[entry.category].emplace_back(entry.name, entry.description);
            }
            std::vector<std::string> lines;
            lines.push_back(COLOR_BOLD "Available functions:" COLOR_RESET);
            for (const auto& [cat, funcs] : categories) {
                lines.push_back(std::string(COLOR_CAT) + "\n[" + cat + "]" + COLOR_RESET);
                for (const auto& [name, desc] : funcs) {
                    lines.push_back("  " COLOR_FUNC + name + COLOR_RESET ": " COLOR_DESC + desc + COLOR_RESET);
                }
            }
            show_paginated(lines, 20);
            continue;
        }

        // Single function help: ?Function
        if (!input.empty() && input[0] == '?') {
            std::string func = input.substr(1);
            func.erase(0, func.find_first_not_of(" \t"));
            func.erase(func.find_last_not_of(" \t") + 1);
            const auto& entries = aleph3::get_help_entries();
            auto it = std::find_if(entries.begin(), entries.end(),
                [&](const aleph3::HelpEntry& e) { return e.name == func; });
            if (it != entries.end()) {
                std::cout << COLOR_FUNC << it->name << COLOR_RESET << ": "
                    << COLOR_DESC << it->description << COLOR_RESET << std::endl;
            }
            else {
                std::cout << COLOR_ERR << "No help available for '" << func << "'." << COLOR_RESET << std::endl;
            }
            continue;
        }

        if (input.starts_with("let ")) {
            // Example: let x = 5
            auto eq_pos = input.find('=');
            if (eq_pos == std::string::npos) {
                std::cout << COLOR_ERR << "Error: Invalid let syntax" << COLOR_RESET << std::endl;
                continue;
            }
            std::string varname = input.substr(4, eq_pos - 4);
            std::string expr_text = input.substr(eq_pos + 1);

            // Trim spaces
            varname.erase(0, varname.find_first_not_of(" \t"));
            varname.erase(varname.find_last_not_of(" \t") + 1);
            expr_text.erase(0, expr_text.find_first_not_of(" \t"));
            expr_text.erase(expr_text.find_last_not_of(" \t") + 1);

            auto expr = parse_expression(expr_text);
            auto result = evaluate(expr, ctx);
            ctx.variables[varname] = result;

            std::cout << COLOR_OUT << "Out[" << counter << "]= " << COLOR_RESET
                << COLOR_FUNC << varname << COLOR_RESET << " = "
                << COLOR_DESC << to_string(result) << COLOR_RESET << std::endl;
            counter++;
            continue;
        }

        try {
            auto expr = parse_expression(input);

            // Handle function definition
            if (auto def = std::get_if<FunctionDefinition>(&*expr)) {
                ctx.user_functions[def->name] = *def;
                std::cout << COLOR_OUT << "Out[" << counter << "]= " << COLOR_RESET
                    << COLOR_DESC << to_string(*expr) << COLOR_RESET << std::endl;
                counter++;
                continue;
            }

            // Check for FullForm[expr]
            if (auto* call = std::get_if<FunctionCall>(&*expr)) {
                if (call->head == "FullForm" && call->args.size() == 1) {
                    std::cout << COLOR_OUT << "Out[" << counter << "]= " << COLOR_RESET
                        << to_fullform(call->args[0]) << std::endl;
                    counter++;
                    continue;
                }
            }

            // Evaluate and simplify expression
            auto result = evaluate(expr, ctx);
            auto simplified = simplify(result);

            std::cout << COLOR_OUT << "Out[" << counter << "]= " << COLOR_RESET;
            if (auto num = std::get_if<Number>(&*simplified)) {
                std::cout << COLOR_NUM << num->value << COLOR_RESET << std::endl;
            }
            else {
                std::cout << COLOR_DESC << to_string(simplified) << COLOR_RESET << std::endl;
            }
        }
        catch (const std::exception& ex) {
            std::cout << COLOR_ERR << "Error: " << ex.what() << COLOR_RESET << std::endl;
        }

        counter++;
    }

    std::cout << COLOR_BOLD << "Goodbye!" << COLOR_RESET << std::endl;
    return 0;
}
