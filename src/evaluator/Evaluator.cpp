#include "evaluator/Evaluator.hpp"
#include "algebra/Polynomial.hpp"
#include "algebra/PolyUtils.hpp"
#include <stdexcept>

namespace aleph3 {

    ExprPtr evaluate_polynomial_function(const FunctionCall& func, EvaluationContext& ctx) {
        const std::string& name = func.head;
        size_t nargs = func.args.size();

        if (name == "Expand") {
            if (nargs != 1) throw std::runtime_error("Expand expects exactly one argument");
            auto arg = evaluate(func.args[0], ctx);
            return expand_polynomial(arg, ctx);
        }
        if (name == "Factor") {
            if (nargs != 1) throw std::runtime_error("Factor expects exactly one argument");
            auto arg = evaluate(func.args[0], ctx);
            return factor_polynomial(arg, ctx);
        }
        if (name == "Collect") {
            if (nargs != 2) throw std::runtime_error("Collect expects exactly two arguments");
            auto arg = evaluate(func.args[0], ctx);
            // You may want to pass the variable as a string or symbol, but for now:
            return collect_polynomial(arg, ctx);
        }
        if (name == "GCD") {
            if (nargs != 2) throw std::runtime_error("GCD expects exactly two arguments");
            auto arg1 = evaluate(func.args[0], ctx);
            auto arg2 = evaluate(func.args[1], ctx);
            return gcd_polynomial(arg1, arg2, ctx);
        }
        if (name == "PolynomialQuotient") {
            if (nargs != 2) throw std::runtime_error("PolynomialQuotient expects exactly two arguments");
            auto dividend = evaluate(func.args[0], ctx);
            auto divisor = evaluate(func.args[1], ctx);
            auto result = divide_polynomial(dividend, divisor, ctx);
            // Return as a list: {quotient, remainder}
            return make_expr<List>(std::vector<ExprPtr>{result.first, result.second});
        }

        throw std::runtime_error("Unknown polynomial function: " + name);
    }

} // namespace aleph3