#include "transforms/Expand.hpp"
#include "expr/Expr.hpp"
#include "util/Utils.hpp"
#include <cmath>

namespace mathix {

    // Recursive expand
    ExprPtr expand(const ExprPtr& expr) {
        if (auto f = std::get_if<FunctionCall>(expr.get())) {
            std::vector<ExprPtr> new_args;
            for (auto& arg : f->args) {
                new_args.push_back(expand(arg));
            }

            if (f->head == "Times" && new_args.size() == 2) {
                auto lhs = new_args[0];
                auto rhs = new_args[1];

                // Expand: (a + b) * c
                if (is_function(lhs, "Plus")) {
                    const auto& lhs_func = std::get<FunctionCall>(*lhs);
                    return make_plus(
                        make_times(lhs_func.args[0], rhs),
                        make_times(lhs_func.args[1], rhs)
                    );
                }

                // Expand: a * (b + c)
                if (is_function(rhs, "Plus")) {
                    const auto& rhs_func = std::get<FunctionCall>(*rhs);
                    return make_plus(
                        make_times(lhs, rhs_func.args[0]),
                        make_times(lhs, rhs_func.args[1])
                    );
                }

                // No expansion possible
                return make_expr<FunctionCall>("Times", new_args);
            }

            if (f->head == "Pow" && new_args.size() == 2) {
                auto base = new_args[0];
                int exp;

                try {
                    exp = get_integer_value(new_args[1]);
                }
                catch (...) {
                    // Return unevaluated if exponent is not an integer
                    return make_expr<FunctionCall>("Pow", new_args);
                }

                // Check for (a + b)^2 pattern
                if (const auto* base_func = std::get_if<FunctionCall>(base.get())) {
                    if (base_func->head == "Plus" && base_func->args.size() == 2 && exp == 2) {
                        auto a = base_func->args[0];
                        auto b = base_func->args[1];

                        return make_plus({
                            make_pow(a, 2),
                            make_times({make_number(2), a, b}),
                            make_pow(b, 2)
                            });
                    }
                }

                // No special expansion case
                return make_expr<FunctionCall>("Pow", new_args);
            }

            return make_expr<FunctionCall>(f->head, new_args);
        }

        return expr; // Base case: atom
    }

}