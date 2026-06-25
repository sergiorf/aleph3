#include "evaluator/EvaluatorAlgebra.hpp"

#include "algebra/PolyUtils.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "evaluator/EvaluatorSemantics.hpp"

#include <algorithm>
#include <functional>
#include <set>
#include <vector>

namespace aleph3 {

namespace {

std::vector<std::string> dedupe_variables(const std::vector<std::string>& variables) {
    std::vector<std::string> deduped;
    for (const auto& variable : variables) {
        if (std::find(deduped.begin(), deduped.end(), variable) == deduped.end()) {
            deduped.push_back(variable);
        }
    }
    return deduped;
}

std::vector<std::string> extract_variables(const ExprPtr& expr) {
    if (auto sym = std::get_if<Symbol>(&(*expr))) {
        return {sym->name};
    }
    if (auto func = std::get_if<FunctionCall>(&(*expr))) {
        if (func->head == "List") {
            std::vector<std::string> vars;
            for (const auto& item : func->args) {
                if (auto s = std::get_if<Symbol>(&(*item))) {
                    vars.push_back(s->name);
                }
                else {
                    throw_invalid_form("Variable list must contain only symbols");
                }
            }
            vars = dedupe_variables(vars);
            if (vars.empty()) {
                throw_invalid_form("Variable list must not be empty");
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
                throw_invalid_form("Variable list must contain only symbols");
            }
        }
        vars = dedupe_variables(vars);
        if (vars.empty()) {
            throw_invalid_form("Variable list must not be empty");
        }
        return vars;
    }
    throw_invalid_form("Variable argument must be a symbol or list of symbols");
}

std::vector<std::string> infer_variables(const ExprPtr& expr) {
    std::set<std::string> vars;
    std::function<void(const ExprPtr&)> visit = [&](const ExprPtr& e) {
        if (!e) return;
        if (auto sym = std::get_if<Symbol>(&(*e))) {
            vars.insert(sym->name);
        }
        else if (auto call = std::get_if<FunctionCall>(&(*e))) {
            for (const auto& arg : call->args) {
                visit(arg);
            }
        }
    };
    visit(expr);
    return {vars.begin(), vars.end()};
}

std::vector<std::string> infer_variables(const ExprPtr& left, const ExprPtr& right) {
    auto variables = infer_variables(left);
    for (const auto& variable : infer_variables(right)) {
        if (std::find(variables.begin(), variables.end(), variable) == variables.end()) {
            variables.push_back(variable);
        }
    }
    return variables;
}

}  // namespace

bool is_symbolic_algebra_function(const std::string& name) {
    return is_algebra_function(name);
}

ExprPtr evaluate_algebra_function(const FunctionCall& func, EvaluationContext& ctx) {
    const std::string& name = func.head;
    const size_t nargs = func.args.size();

    if (name == "Expand") {
        if (nargs != 1) throw_invalid_arity_exact("Expand", 1);
        return expand_polynomial(func.args[0], ctx);
    }
    if (name == "Factor") {
        if (nargs != 1) throw_invalid_arity_exact("Factor", 1);
        return factor_polynomial(func.args[0], ctx);
    }
    if (name == "Collect") {
        if (nargs != 2) throw_invalid_arity_exact("Collect", 2);
        const auto variables = extract_variables(func.args[1]);
        return collect_polynomial(func.args[0], variables, ctx);
    }
    if (name == "GCD") {
        if (nargs != 2 && nargs != 3) {
            throw_invalid_arity_between("GCD", 2, 3);
        }
        const auto variables =
            nargs == 3 ? extract_variables(func.args[2]) : infer_variables(func.args[0], func.args[1]);
        return gcd_polynomial(func.args[0], func.args[1], variables, ctx);
    }
    if (name == "PolynomialQuotient") {
        if (nargs != 2 && nargs != 3) {
            throw_invalid_arity_between("PolynomialQuotient", 2, 3);
        }
        const auto variables =
            nargs == 3 ? extract_variables(func.args[2]) : infer_variables(func.args[0], func.args[1]);
        const auto result = divide_polynomial(func.args[0], func.args[1], variables, ctx);
        return make_expr<List>(std::vector<ExprPtr>{result.first, result.second});
    }

    throw_unsupported_construct("Unknown algebra function: " + name);
}

}  // namespace aleph3
