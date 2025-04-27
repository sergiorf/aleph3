#include "expr/Expr.hpp"
#include "evaluator/Evaluator.hpp"
#include "parser/Parser.hpp"

#include <iostream>
#include <string>

using namespace mathix;

int main() {
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

        try {
            auto expr = parse_expression(input);
            auto result = evaluate(expr);
            std::cout << "= " << to_string(result) << std::endl;
        }
        catch (const std::exception& ex) {
            std::cout << "Error: " << ex.what() << std::endl;
        }
    }

    std::cout << "Goodbye!" << std::endl;
    return 0;
}
