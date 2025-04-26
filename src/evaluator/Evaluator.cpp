#include "evaluator/Evaluator.hpp"

namespace mathix {

// Helper: overloaded visitor
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// Internal helper: evaluates a function call like Plus[2, 3]
ExprPtr evaluate_function(const FunctionCall& func) {
    if (func.head == "Plus") {
        double sum = 0.0;
        for (const auto& arg : func.args) {
            if (auto num = std::get_if<Number>(&(*arg))) {
                sum += num->value;
            } else {
                // If non-number arguments exist, keep unevaluated
                return std::make_shared<Expr>(func);
            }
        }
        return make_expr<Number>(sum);
    }
    else if (func.head == "Times") {
        double product = 1.0;
        for (const auto& arg : func.args) {
            if (auto num = std::get_if<Number>(&(*arg))) {
                product *= num->value;
            } else {
                // If non-number arguments exist, keep unevaluated
                return std::make_shared<Expr>(func);
            }
        }
        return make_expr<Number>(product);
    }
    else {
        // Unknown function: leave it unevaluated
        return std::make_shared<Expr>(func);
    }
}

ExprPtr evaluate(const ExprPtr& expr) {
    return std::visit(overloaded {
        [](const Number& num) -> ExprPtr {
            return std::make_shared<Expr>(num); // Numbers are already evaluated
        },
        [](const Symbol& sym) -> ExprPtr {
            return std::make_shared<Expr>(sym); // Symbols are not evaluated here
        },
        [](const FunctionCall& func) -> ExprPtr {
            return evaluate_function(func);
        }
    }, *expr);
}

} // namespace mathix
