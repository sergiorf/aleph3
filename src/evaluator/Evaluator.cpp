#include "evaluator/Evaluator.hpp"
#include "algebra/Polynomial.hpp"
#include "algebra/PolyUtils.hpp"
#include <stdexcept>
#include <vector>
#include <string>
#include <set>

namespace aleph3 {

    // Helper: Extract variable(s) from an ExprPtr argument (for univariate, expects a Symbol or List of Symbols)
    std::vector<std::string> extract_variables(const ExprPtr& expr) {
        if (auto sym = std::get_if<Symbol>(&(*expr))) {
            return { sym->name };
        }
        if (auto func = std::get_if<FunctionCall>(&(*expr))) {
            if (func->head == "List") {
                std::vector<std::string> vars;
                for (const auto& item : func->args) {
                    if (auto s = std::get_if<Symbol>(&(*item))) {
                        vars.push_back(s->name);
                    }
                    else {
                        throw std::runtime_error("Variable list must contain only symbols");
                    }
                }
                return vars;
            }
        }
        if (auto list = std::get_if<List>(&(*expr))) {
            std::vector<std::string> vars;
            for (const auto& item : list->elements) {
                if (auto s = std::get_if<Symbol>(&(*item))) {
                    vars.push_back(s->name);
                }
                else {
                    throw std::runtime_error("Variable list must contain only symbols");
                }
            }
            return vars;
        }
        throw std::runtime_error("Variable argument must be a symbol or list of symbols");
    }

    ExprPtr evaluate_polynomial_function(const FunctionCall& func, EvaluationContext& ctx) {
        const std::string& name = func.head;
        size_t nargs = func.args.size();

        if (name == "Expand") {
            if (nargs != 1) throw std::runtime_error("Expand expects exactly one argument");
            return expand_polynomial(func.args[0], ctx);
        }
        if (name == "Factor") {
            if (nargs != 1) throw std::runtime_error("Factor expects exactly one argument");
            return factor_polynomial(func.args[0], ctx);
        }
        if (name == "Collect") {
            if (nargs != 2) throw std::runtime_error("Collect expects exactly two arguments");
            auto variables = extract_variables(func.args[1]);
            return collect_polynomial(func.args[0], variables, ctx);
        }
        if (name == "GCD") {
            if (nargs != 2 && nargs != 3) {
                throw std::runtime_error("GCD expects two polynomial arguments and an optional variable selector");
            }
            std::vector<std::string> variables;
            if (nargs == 3) {
                variables = extract_variables(func.args[2]);
            } else {
                // Infer variables from the first argument for compatibility.
                std::set<std::string> vars;
                std::function<void(const ExprPtr&)> visit = [&](const ExprPtr& e) {
                    if (!e) return;
                    if (auto sym = std::get_if<Symbol>(&(*e))) {
                        vars.insert(sym->name);
                    }
                    else if (auto call = std::get_if<FunctionCall>(&(*e))) {
                        for (const auto& a : call->args) visit(a);
                    }
                };
                visit(func.args[0]);
                variables.assign(vars.begin(), vars.end());
            }
            return gcd_polynomial(func.args[0], func.args[1], variables, ctx);
        }
        if (name == "PolynomialQuotient") {
            if (nargs != 2 && nargs != 3) {
                throw std::runtime_error(
                    "PolynomialQuotient expects dividend, divisor, and an optional variable selector");
            }
            std::vector<std::string> variables;
            if (nargs == 3) {
                variables = extract_variables(func.args[2]);
            } else {
                // Infer variables from the dividend for compatibility.
                std::set<std::string> vars;
                std::function<void(const ExprPtr&)> visit = [&](const ExprPtr& e) {
                    if (!e) return;
                    if (auto sym = std::get_if<Symbol>(&(*e))) {
                        vars.insert(sym->name);
                    }
                    else if (auto call = std::get_if<FunctionCall>(&(*e))) {
                        for (const auto& a : call->args) visit(a);
                    }
                };
                visit(func.args[0]);
                variables.assign(vars.begin(), vars.end());
            }
            auto result = divide_polynomial(func.args[0], func.args[1], variables, ctx);
            // Return as a list: {quotient, remainder}
            return make_expr<List>(std::vector<ExprPtr>{result.first, result.second});
        }

        throw std::runtime_error("Unknown polynomial function: " + name);
    }

} // namespace aleph3
