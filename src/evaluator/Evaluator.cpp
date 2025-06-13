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
            auto var_arg = evaluate(func.args[1], ctx);
            auto variables = extract_variables(var_arg);
            return collect_polynomial(arg, variables, ctx);
        }
        if (name == "GCD") {
            if (nargs != 2) throw std::runtime_error("GCD expects exactly two arguments");
            auto arg1 = evaluate(func.args[0], ctx);
            auto arg2 = evaluate(func.args[1], ctx);
            // For GCD, require user to specify variables as a third argument for multivariate, or infer from args
            // Here we infer from arg1 (could be improved)
            std::set<std::string> vars;
            std::function<void(const ExprPtr&)> visit = [&](const ExprPtr& e) {
                if (!e) return;
                if (auto sym = std::get_if<Symbol>(&(*e))) {
                    vars.insert(sym->name);
                }
                else if (auto func = std::get_if<FunctionCall>(&(*e))) {
                    for (const auto& a : func->args) visit(a);
                }
                };
            visit(arg1);
            std::vector<std::string> variables(vars.begin(), vars.end());
            return gcd_polynomial(arg1, arg2, variables, ctx);
        }
        if (name == "PolynomialQuotient") {
            if (nargs != 2) throw std::runtime_error("PolynomialQuotient expects exactly two arguments");
            auto dividend = evaluate(func.args[0], ctx);
            auto divisor = evaluate(func.args[1], ctx);
            // Infer variables from dividend
            std::set<std::string> vars;
            std::function<void(const ExprPtr&)> visit = [&](const ExprPtr& e) {
                if (!e) return;
                if (auto sym = std::get_if<Symbol>(&(*e))) {
                    vars.insert(sym->name);
                }
                else if (auto func = std::get_if<FunctionCall>(&(*e))) {
                    for (const auto& a : func->args) visit(a);
                }
                };
            visit(dividend);
            std::vector<std::string> variables(vars.begin(), vars.end());
            auto result = divide_polynomial(dividend, divisor, variables, ctx);
            // Return as a list: {quotient, remainder}
            return make_expr<List>(std::vector<ExprPtr>{result.first, result.second});
        }

        throw std::runtime_error("Unknown polynomial function: " + name);
    }

} // namespace aleph3