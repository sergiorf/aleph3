#include "expr/Expr.hpp"
#include "evaluator/Evaluator.hpp"
#include "evaluator/EvaluationContext.hpp"
#include "parser/Parser.hpp"

#include <iostream>
#include <string>

using namespace mathix;

int main() {
    EvaluationContext ctx;

    std::cout << "Welcome to Mathix CLI!" << std::endl;
    std::cout << "Type 'exit' to quit." << std::endl;

    while (true) {
        std::cout << "> ";
        std::string input;
        if (!std::getline(std::cin, input)) {
            break; // End of input (Ctrl+D or error)
        }

        if (input == "exit") {
            break;
        }

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
        
            if (auto num = std::get_if<Number>(&*result)) {
                ctx.variables[varname] = num->value;
                std::cout << varname << " = " << num->value << std::endl;
            } else {
                std::cout << "Error: Can only assign numbers for now" << std::endl;
            }
            continue;
        }
        else {
            try {
                auto expr = parse_expression(input);
                auto result = evaluate(expr, ctx);
    
                if (auto num = std::get_if<Number>(&*result)) {
                    std::cout << "= " << num->value << std::endl;
                }
                else {
                    std::cout << "= " << to_string(result) << std::endl;
                }
            }
            catch (const std::exception& ex) {
                std::cout << "Error: " << ex.what() << std::endl;
            }
        }
    }

    std::cout << "Goodbye!" << std::endl;
    return 0;
}
