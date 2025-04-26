#include "expr/Expr.hpp"
#include "evaluator/Evaluator.hpp"

using namespace mathix;

int main() {
    auto expr2 = make_expr<Number>(2.0);
    /*
    auto expr = make_expr<FunctionCall>(std::string("Plus"), {
        make_expr<Number>(2),
        make_expr<Number>(3)
    });
    */

    std::cout << "Input: " << to_string(expr2) << std::endl;

    auto result = evaluate(expr2);
    std::cout << "Result: " << to_string(result) << std::endl;
}
