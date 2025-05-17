#include "expr/Expr.hpp"
#include "evaluator/Evaluator.hpp"
#include "evaluator/EvaluationContext.hpp"
#include "evaluator/BuiltInFunctions.hpp"
#include "parser/Parser.hpp"

#include <iostream>
#include <string>

using namespace mathix;

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

int main() {
    EvaluationContext ctx;
    int counter = 1;

    mathix::register_built_in_functions();

    std::cout << "Welcome to Mathix CLI!" << std::endl;
    std::cout << "Type 'exit' to quit." << std::endl;

    while (true) {
        std::cout << "In[" << counter << "]:= ";
        std::string input;
        if (!std::getline(std::cin, input)) break;
        if (input == "exit") break;
        if (input.empty()) continue;

        if (input.starts_with("let ")) {
            // Example: let x = 5
            auto eq_pos = input.find('=');
            if (eq_pos == std::string::npos) {
                std::cout << "Error: Invalid let syntax" << std::endl;
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

            std::cout << "Out[" << counter << "]= " << varname << " = " << to_string(result) << std::endl;
            counter++;
            continue;
        }

        try {
            auto expr = parse_expression(input);

            // Handle function definition
            if (auto def = std::get_if<FunctionDefinition>(&*expr)) {
                ctx.user_functions[def->name] = *def;
                std::cout << "Out[" << counter << "]= " << to_string(*expr) << std::endl;
                counter++;
                continue;
            }

            // Evaluate and simplify expression
            auto result = evaluate(expr, ctx);
            auto simplified = simplify(result);

            std::cout << "Out[" << counter << "]= ";
            if (auto num = std::get_if<Number>(&*simplified)) {
                std::cout << num->value << std::endl;
            }
            else {
                std::cout << to_string(simplified) << std::endl;
            }
        }
        catch (const std::exception& ex) {
            std::cout << "Error: " << ex.what() << std::endl;
        }

        counter++;
    }

    std::cout << "Goodbye!" << std::endl;
    return 0;
}

