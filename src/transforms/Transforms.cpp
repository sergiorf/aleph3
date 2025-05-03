#include "transforms/Transforms.hpp"
#include "expr/Expr.hpp"
#include "util/ExprUtils.hpp"

#include <cmath>
#include <map>
#include <algorithm>

namespace mathix {

    // Simplify trivial cases
    ExprPtr simplify(const ExprPtr& expr) {
        if (auto f = std::get_if<FunctionCall>(expr.get())) {
            if (f->head == "Times") {
                std::map<std::string, int> symbol_counts;
                double coefficient = 1.0;
                std::vector<ExprPtr> others;

                for (const auto& arg : f->args) {
                    auto simplified_arg = simplify(arg); // recursively simplify
                    if (auto num = std::get_if<Number>(simplified_arg.get())) {
                        coefficient *= num->value;
                    }
                    else if (auto sym = std::get_if<Symbol>(simplified_arg.get())) {
                        symbol_counts[sym->name] += 1;
                    }
                    else if (auto pow = std::get_if<FunctionCall>(simplified_arg.get())) {
                        if (pow->head == "Pow") {
                            if (auto base = std::get_if<Symbol>(pow->args[0].get())) {
                                if (auto exp = std::get_if<Number>(pow->args[1].get())) {
                                    symbol_counts[base->name] += static_cast<int>(exp->value);
                                    continue;
                                }
                            }
                        }
                        others.push_back(simplified_arg);
                    }
                    else {
                        others.push_back(simplified_arg);
                    }
                }

                std::vector<ExprPtr> result;
                if (coefficient != 1.0 || (symbol_counts.empty() && others.empty())) {
                    result.push_back(make_number(coefficient));
                }

                // Reconstruct Pow terms
                std::vector<std::string> ordered_symbols;
                for (const auto& [name, _] : symbol_counts) {
                    ordered_symbols.push_back(name);
                }
                std::sort(ordered_symbols.begin(), ordered_symbols.end());

                for (const auto& name : ordered_symbols) {
                    int count = symbol_counts[name];
                    ExprPtr sym = make_expr<Symbol>(name);
                    if (count == 1) {
                        result.push_back(sym);
                    }
                    else {
                        result.push_back(make_pow(sym, count));
                    }
                }

                result.insert(result.end(), others.begin(), others.end());

                if (result.size() == 1) return result[0];
                return make_expr<FunctionCall>("Times", result);
            }

            if (f->head == "Pow") {
                auto base = f->args[0];
                auto exponent = f->args[1];

                // Simplify 1^n → 1
                if (is_one(base)) {
                    return make_number(1);
                }

                // Simplify x^1 → x
                if (auto num = std::get_if<Number>(exponent.get())) {
                    if (num->value == 1.0) {
                        return base;
                    }
                }

                // Simplify n^m where n and m are numbers
                if (auto base_num = std::get_if<Number>(base.get())) {
                    if (auto exp_num = std::get_if<Number>(exponent.get())) {
                        return make_number(std::pow(base_num->value, exp_num->value));
                    }
                }

                // Simplify (a * b)^n → a^n * b^n
                if (auto base_func = std::get_if<FunctionCall>(base.get())) {
                    if (base_func->head == "Times") {
                        std::vector<ExprPtr> expanded_terms;
                        for (const auto& term : base_func->args) {
                            expanded_terms.push_back(make_pow(term, get_integer_value(exponent)));
                        }
                        return simplify(make_expr<FunctionCall>("Times", expanded_terms));
                    }
                }
            }

            if (f->head == "Plus") {
                std::vector<ExprPtr> simplified_args;
                for (const auto& arg : f->args) {
                    simplified_args.push_back(simplify(arg));
                }

                // Combine like terms (e.g., 2 * x + 3 * x → 5 * x)
                std::map<std::string, double> term_coefficients;
                std::vector<ExprPtr> non_numeric_terms;

                for (const auto& arg : simplified_args) {
                    if (auto times_func = std::get_if<FunctionCall>(arg.get())) {
                        if (times_func->head == "Times" && times_func->args.size() == 2) {
                            if (auto coeff = std::get_if<Number>(times_func->args[0].get())) {
                                if (auto symbol = std::get_if<Symbol>(times_func->args[1].get())) {
                                    term_coefficients[symbol->name] += coeff->value;
                                    continue;
                                }
                            }
                        }
                    } else if (auto symbol = std::get_if<Symbol>(arg.get())) {
                        term_coefficients[symbol->name] += 1.0;
                        continue;
                    } else if (auto num = std::get_if<Number>(arg.get())) {
                        term_coefficients[""] += num->value; // Use empty string for constants
                        continue;
                    }
                    non_numeric_terms.push_back(arg);
                }

                for (const auto& [symbol, coeff] : term_coefficients) {
                    if (!symbol.empty()) {
                        non_numeric_terms.push_back(make_times(make_number(coeff), make_expr<Symbol>(symbol)));
                    } else {
                        non_numeric_terms.push_back(make_number(coeff));
                    }
                }

                // Sort terms: symbols first, constants last
                std::sort(non_numeric_terms.begin(), non_numeric_terms.end(), [](const ExprPtr& a, const ExprPtr& b) {
                    auto degree = [](const ExprPtr& term) -> int {
                        if (auto pow = std::get_if<FunctionCall>(term.get())) {
                            if (pow->head == "Pow") {
                                if (auto exp = std::get_if<Number>(pow->args[1].get())) {
                                    return static_cast<int>(exp->value);
                                }
                            }
                            if (pow->head == "Times") {
                                for (auto& arg : pow->args) {
                                    if (auto inner_pow = std::get_if<FunctionCall>(arg.get())) {
                                        if (inner_pow->head == "Pow") {
                                            if (auto exp = std::get_if<Number>(inner_pow->args[1].get())) {
                                                return static_cast<int>(exp->value);
                                            }
                                        }
                                    }
                                    else if (std::holds_alternative<Symbol>(*arg)) {
                                        return 1; // x is degree 1
                                    }
                                }
                            }
                        }
                        else if (std::holds_alternative<Symbol>(*term)) {
                            return 1;
                        }
                        else if (std::holds_alternative<Number>(*term)) {
                            return 0;
                        }
                        return -1; // unknown terms last
                        };

                    int da = degree(a);
                    int db = degree(b);
                    if (da != db) return da > db;

                    return to_string(a) < to_string(b); // tie-breaker: lex order
                });

                return make_expr<FunctionCall>("Plus", non_numeric_terms);
            }
        }
        return expr; // Return the original expression if no simplification is possible
    }

    // Recursive expand
    ExprPtr expand(const ExprPtr& expr) {
        if (auto f = std::get_if<FunctionCall>(expr.get())) {
            std::vector<ExprPtr> new_args;
            for (auto& arg : f->args) {
                new_args.push_back(expand(arg)); // Recursively expand arguments
            }

            if (f->head == "Times" && new_args.size() == 2) {
                auto lhs = new_args[0];
                auto rhs = new_args[1];

                // Expand: (a + b) * (c + d)
                if (is_function(lhs, "Plus") && is_function(rhs, "Plus")) {
                    const auto& lhs_func = std::get<FunctionCall>(*lhs);
                    const auto& rhs_func = std::get<FunctionCall>(*rhs);

                    std::vector<ExprPtr> distributed_terms;
                    for (const auto& lhs_arg : lhs_func.args) {
                        for (const auto& rhs_arg : rhs_func.args) {
                            distributed_terms.push_back(simplify(make_times(lhs_arg, rhs_arg)));
                        }
                    }
                    return simplify(make_plus(distributed_terms));
                }

                // Expand: (a + b) * c
                if (is_function(lhs, "Plus")) {
                    const auto& lhs_func = std::get<FunctionCall>(*lhs);
                    return simplify(make_plus({
                        make_times(lhs_func.args[0], rhs),
                        make_times(lhs_func.args[1], rhs)
                        }));
                }

                // Expand: a * (b + c)
                if (is_function(rhs, "Plus")) {
                    const auto& rhs_func = std::get<FunctionCall>(*rhs);
                    return simplify(make_plus({
                        make_times(lhs, rhs_func.args[0]),
                        make_times(lhs, rhs_func.args[1])
                        }));
                }

                // No expansion possible
                return simplify(make_expr<FunctionCall>("Times", new_args));
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

                        return simplify(make_plus({
                            make_pow(a, 2),                           // a^2
                            make_times({make_number(2), a, b}),       // 2 * a * b
                            make_pow(b, 2)                            // b^2
                            }));
                    }
                }

                // No special expansion case
                return simplify(make_expr<FunctionCall>("Pow", new_args));
            }

            return simplify(make_expr<FunctionCall>(f->head, new_args));
        }

        return expr; // Base case: atom
    }


}