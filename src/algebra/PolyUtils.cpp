#include "algebra/PolyUtils.hpp"
#include "algebra/Polynomial.hpp"
#include "expr/Expr.hpp"
#include "expr/ExprUtils.hpp"
#include <stdexcept>
#include <utility>
#include <set>
#include <algorithm>
#include <functional>

namespace aleph3 {

    // --- Conversion utilities ---

    // Convert ExprPtr to Polynomial (supports multivariate, basic Plus/Times/Power/Number/Symbol)
    Polynomial expr_to_polynomial(const ExprPtr& expr, const std::vector<std::string>& variables) {
        if (!expr) throw std::runtime_error("Null expression");

        // Helper: build a monomial from a variable->exponent map
        auto make_monomial = [&](const std::map<std::string, int>& exps) -> Monomial {
            Monomial m;
            for (const auto& v : variables) {
                auto it = exps.find(v);
                if (it != exps.end() && it->second != 0)
                    m[v] = it->second;
            }
            return m;
            };

        // Recursive lambda
        std::function<Polynomial(const ExprPtr&)> recur = [&](const ExprPtr& e) -> Polynomial {
            if (auto num = std::get_if<Number>(&(*e))) {
                return Polynomial(static_cast<double>(num->value));
            }
            if (auto sym = std::get_if<Symbol>(&(*e))) {
                std::map<std::string, int> exps;
                exps[sym->name] = 1;
                return Polynomial({ {make_monomial(exps), 1.0} });
            }
            if (auto plus = std::get_if<FunctionCall>(&(*e)); plus && plus->head == "Plus") {
                Polynomial result;
                for (const auto& arg : plus->args) {
                    result = result + recur(arg);
                }
                return result;
            }
            if (auto times = std::get_if<FunctionCall>(&(*e)); times && times->head == "Times") {
                Polynomial result(1.0);
                for (const auto& arg : times->args) {
                    result = result * recur(arg);
                }
                return result;
            }
            if (auto pow = std::get_if<FunctionCall>(&(*e)); pow && pow->head == "Power") {
                if (pow->args.size() == 2) {
                    auto base = pow->args[0];
                    auto exp = pow->args[1];
                    if (auto s = std::get_if<Symbol>(&(*base))) {
                        if (auto n = std::get_if<Number>(&(*exp))) {
                            std::map<std::string, int> exps;
                            exps[s->name] = static_cast<int>(n->value);
                            return Polynomial({ {make_monomial(exps), 1.0} });
                        }
                    }
                }
            }
            throw std::runtime_error("expr_to_polynomial: Not implemented for this expression");
            };

        return recur(expr);
    }

    // Convert Polynomial to ExprPtr (sum of terms, supports multivariate)
    ExprPtr polynomial_to_expr(const Polynomial& poly) {
        std::vector<ExprPtr> terms;
        for (const auto& [mono, coeff] : poly.terms) {
            if (std::abs(coeff) < 1e-10) continue;
            ExprPtr term = make_expr<Number>(coeff);
            for (const auto& [var, exp] : mono) {
                ExprPtr v = make_expr<Symbol>(var);
                if (exp == 1) {
                    term = make_fcall("Times", { term, v });
                }
                else {
                    term = make_fcall("Times", { term, make_fcall("Power", {v, make_expr<Number>(static_cast<double>(exp))}) });
                }
            }
            terms.push_back(term);
        }
        if (terms.empty()) return make_expr<Number>(0.0);
        if (terms.size() == 1) return terms[0];
        return make_expr<FunctionCall>("Plus", terms);
    }

    // --- High-level API ---

    ExprPtr expand_polynomial(const ExprPtr& expr, EvaluationContext& ctx) {
        // Try to infer variables from expr (collect all symbols)
        std::set<std::string> vars;
        std::function<void(const ExprPtr&)> visit = [&](const ExprPtr& e) {
            if (!e) return;
            if (auto sym = std::get_if<Symbol>(&(*e))) {
                vars.insert(sym->name);
            }
            else if (auto func = std::get_if<FunctionCall>(&(*e))) {
                for (const auto& arg : func->args) visit(arg);
            }
            };
        visit(expr);
        std::vector<std::string> variables(vars.begin(), vars.end());

        Polynomial poly = expr_to_polynomial(expr, variables);
        Polynomial expanded = expand(poly);
        return polynomial_to_expr(expanded);
    }

    ExprPtr factor_polynomial(const ExprPtr& expr, EvaluationContext& ctx) {
        std::set<std::string> vars;
        std::function<void(const ExprPtr&)> visit = [&](const ExprPtr& e) {
            if (!e) return;
            if (auto sym = std::get_if<Symbol>(&(*e))) {
                vars.insert(sym->name);
            }
            else if (auto func = std::get_if<FunctionCall>(&(*e))) {
                for (const auto& arg : func->args) visit(arg);
            }
            };
        visit(expr);
        std::vector<std::string> variables(vars.begin(), vars.end());

        Polynomial poly = expr_to_polynomial(expr, variables);
        Polynomial factored = factor(poly);
        return polynomial_to_expr(factored);
    }

    ExprPtr collect_polynomial(const ExprPtr& expr, const std::vector<std::string>& variables, EvaluationContext& ctx) {
        Polynomial poly = expr_to_polynomial(expr, variables);
        Polynomial collected = collect(poly, variables);
        return polynomial_to_expr(collected);
    }

    ExprPtr gcd_polynomial(const ExprPtr& a, const ExprPtr& b, const std::vector<std::string>& variables, EvaluationContext& ctx) {
        Polynomial pa = expr_to_polynomial(a, variables);
        Polynomial pb = expr_to_polynomial(b, variables);
        Polynomial g = gcd(pa, pb, variables);
        return polynomial_to_expr(g);
    }

    std::pair<ExprPtr, ExprPtr> divide_polynomial(const ExprPtr& dividend, const ExprPtr& divisor, const std::vector<std::string>& variables, EvaluationContext& ctx) {
        Polynomial pdiv = expr_to_polynomial(dividend, variables);
        Polynomial pdis = expr_to_polynomial(divisor, variables);
        auto result = divide(pdiv, pdis, variables);
        return { polynomial_to_expr(result.first), polynomial_to_expr(result.second) };
    }

    // --- Low-level API ---

    Polynomial expand(const Polynomial& poly) {
        // Already expanded in this representation
        return poly;
    }

    Polynomial factor(const Polynomial& poly) {
        // Placeholder: no actual factorization
        return poly;
    }

    Polynomial collect(const Polynomial& poly, const std::vector<std::string>& variables) {
        // Placeholder: no actual collection, just returns input
        return poly;
    }

    Polynomial gcd(const Polynomial& a, const Polynomial& b, const std::vector<std::string>& variables) {
        // Only supports univariate GCD for now
        if (variables.size() != 1)
            throw std::runtime_error("gcd: only univariate GCD is implemented");
        return Polynomial::gcd(a, b, variables[0]);
    }

    std::pair<Polynomial, Polynomial> divide(const Polynomial& dividend, const Polynomial& divisor, const std::vector<std::string>& variables) {
        // Only supports univariate division for now
        if (variables.size() != 1)
            throw std::runtime_error("divide: only univariate division is implemented");
        return dividend.divide(divisor);
    }

} // namespace aleph3