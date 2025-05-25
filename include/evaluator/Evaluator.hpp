// evaluator/Evaluator.hpp
#pragma once

#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"
#include "evaluator/FunctionRegistry.hpp"
#include "util/Overloaded.hpp"
#include "expr/ExprUtils.hpp"
#include "transforms/Transforms.hpp"
#include "ExtraMath.hpp"
#include "Constants.hpp"

#include <stdexcept>
#include <cmath>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <unordered_set>

namespace aleph3 {

ExprPtr evaluate(const ExprPtr& expr, EvaluationContext& ctx);

inline std::string expr_to_key(const ExprPtr& expr) {
    if (std::holds_alternative<Number>(*expr)) {
        double val = std::get<Number>(*expr).value;
        if (val == 0.0) return "0";
        if (val == PI) return "Pi";
        return std::to_string(val);
    }
    if (std::holds_alternative<Symbol>(*expr)) {
        return std::get<Symbol>(*expr).name;
    }
    if (std::holds_alternative<FunctionCall>(*expr)) {
        const auto& call = std::get<FunctionCall>(*expr);
        if (call.head == "Divide" && call.args.size() == 2) {
            return expr_to_key(call.args[0]) + "/" + expr_to_key(call.args[1]);
        }
    }
    return ""; // Not a known key
}

inline ExprPtr evaluate_function(const FunctionCall& func, EvaluationContext& ctx) {
    const std::string& name = func.head;
    size_t nargs = func.args.size();

    // 1. Try FunctionRegistry (for extensible built-ins)
    auto& registry = FunctionRegistry::instance();
    if (registry.has_function(name)) {
        auto handler = registry.get_function(name);
        return handler(func, ctx);
    }

    // 2. Special forms
    if (name == "If") {
        if (nargs != 3) throw std::runtime_error("If expects exactly 3 arguments");
        auto condition = evaluate(func.args[0], ctx);
        if (std::holds_alternative<Boolean>(*condition)) {
            bool cond = std::get<Boolean>(*condition).value;
            return cond ? evaluate(func.args[1], ctx) : evaluate(func.args[2], ctx);
        }
        return make_expr<FunctionCall>(name, func.args);
    }
    if (name == "Expand") {
        if (nargs != 1) throw std::runtime_error("Expand expects exactly one argument");
        auto arg = evaluate(func.args[0], ctx);
        return expand(arg);
    }
    if (name == "Negate") {
        if (nargs != 1) throw std::runtime_error("Negate expects exactly 1 argument");
        auto arg = evaluate(func.args[0], ctx);
        if (std::holds_alternative<Number>(*arg)) {
            return make_expr<Number>(-get_number_value(arg));
        }
        return make_expr<FunctionCall>("Times", std::vector{ make_expr<Number>(-1), arg });
    }

    // 3. Built-in function maps
    static const std::unordered_map<std::string, std::function<double(double)>> unary_functions = {
        {"Sin",   [](double x) { return std::sin(x); }},
        {"Cos",   [](double x) { return std::cos(x); }},
        {"Tan",   [](double x) { return std::tan(x); }},
        {"Csc",   [](double x) { return csc(x); }},
        {"Sec",   [](double x) { return sec(x); }},
        {"Sinh",  [](double x) { return std::sinh(x); }},
        {"Cosh",  [](double x) { return std::cosh(x); }},
        {"Tanh",  [](double x) { return std::tanh(x); }},
        {"Coth",  [](double x) { return coth(x); }},
        {"Sech",  [](double x) { return sech(x); }},
        {"Csch",  [](double x) { return csch(x); }},
        {"Cot",   [](double x) { return cot(x); }},
        {"Abs",   [](double x) { return std::fabs(x); }},
        {"Sqrt",  [](double x) { return std::sqrt(x); }},
        {"Exp",   [](double x) { return std::exp(x); }},
        {"Log",   [](double x) { return std::log(x); }},
        {"Floor", [](double x) { return std::floor(x); }},
        {"Ceiling",[](double x) { return std::ceil(x); }},
        {"Round", [](double x) { return std::round(x); }},
        {"ArcSin",[](double x) { return std::asin(x); }},
        {"ArcCos",[](double x) { return std::acos(x); }},
        {"ArcTan",[](double x) { return std::atan(x); }}
    };
    static const std::unordered_map<std::string, std::function<double(double, double)>> binary_functions = {
        {"Plus",   [](double a, double b) { return a + b; }},
        {"Minus",  [](double a, double b) { return a - b; }},
        {"Times",  [](double a, double b) { return a * b; }},
        {"Divide", [](double a, double b) { return a / b; }},
        {"Power",  [](double a, double b) { return std::pow(a, b); }},
        {"Log",    [](double b, double x) { return std::log(x) / std::log(b); }},
        {"ArcTan", [](double x, double y) { return std::atan2(y, x); }}
    };
    static const std::unordered_map<std::string, std::function<bool(double, double)>> comparison_functions = {
        {"Equal",         [](double a, double b) { return a == b; }},
        {"NotEqual",      [](double a, double b) { return a != b; }},
        {"Less",          [](double a, double b) { return a < b; }},
        {"Greater",       [](double a, double b) { return a > b; }},
        {"LessEqual",     [](double a, double b) { return a <= b; }},
        {"GreaterEqual",  [](double a, double b) { return a >= b; }}
    };

    static const std::unordered_map<std::string, std::unordered_map<std::string, double>> known_symbolic_unary = {
        {"Sin", {
            {"0", 0.0},
            {"Pi", 0.0},
            {"2*Pi", 0.0},
            {"-Pi", 0.0},
            {"Pi/2", 1.0},
            {"-Pi/2", -1.0},
            {"Pi/4", std::sqrt(2.0) / 2.0},
            {"-Pi/4", -std::sqrt(2.0) / 2.0},
            {"3*Pi/2", -1.0},
            {"-3*Pi/2", 1.0}
        }},
        {"Cos", {
            {"0", 1.0},
            {"Pi", -1.0},
            {"2*Pi", 1.0},
            {"-Pi", -1.0},
            {"Pi/2", 0.0},
            {"-Pi/2", 0.0},
            {"Pi/4", std::sqrt(2.0) / 2.0},
            {"-Pi/4", std::sqrt(2.0) / 2.0},
            {"3*Pi/2", 0.0},
            {"-3*Pi/2", 0.0}
        }},
        {"Tan", {
            {"0", 0.0},
            {"Pi", 0.0},
            {"2*Pi", 0.0},
            {"-Pi", 0.0},
            {"Pi/4", 1.0},
            {"-Pi/4", -1.0},
            {"Pi/6", std::tan(PI / 6)},
            {"-Pi/6", std::tan(-PI / 6)},
            {"Pi/3", std::tan(PI / 3)},
            {"-Pi/3", std::tan(-PI / 3)}
        }},
        {"Sinc", {
            {"0", 1.0},
            {"Pi", std::sin(PI) / PI},
            {"-Pi", std::sin(-PI) / -PI},
            {"2*Pi", std::sin(2 * PI) / (2 * PI)},
            {"-2*Pi", std::sin(-2 * PI) / (-2 * PI)}
        }}
    };

    // 4. Elementwise/broadcasted binary operations
    auto elementwise = [&ctx](const std::string& op, const ExprPtr& a, const ExprPtr& b) -> ExprPtr {
        if (std::holds_alternative<List>(*a) && std::holds_alternative<List>(*b)) {
            const auto& l1 = std::get<List>(*a).elements;
            const auto& l2 = std::get<List>(*b).elements;
            if (l1.size() != l2.size())
                throw std::runtime_error("List sizes must match for elementwise operation");
            std::vector<ExprPtr> result;
            for (size_t i = 0; i < l1.size(); ++i) {
                result.push_back(evaluate(make_fcall(op, { l1[i], l2[i] }), ctx));
            }
            return std::make_shared<Expr>(List{ result });
        }
        if (std::holds_alternative<List>(*a)) {
            const auto& l1 = std::get<List>(*a).elements;
            std::vector<ExprPtr> result;
            for (const auto& elem : l1) {
                result.push_back(evaluate(make_fcall(op, { elem, b }), ctx));
            }
            return std::make_shared<Expr>(List{ result });
        }
        if (std::holds_alternative<List>(*b)) {
            const auto& l2 = std::get<List>(*b).elements;
            std::vector<ExprPtr> result;
            for (const auto& elem : l2) {
                result.push_back(evaluate(make_fcall(op, { a, elem }), ctx));
            }
            return std::make_shared<Expr>(List{ result });
        }
        return nullptr;
        };

    // 5. Built-in unary
    if (nargs == 1) {
        auto it = unary_functions.find(name);
        if (it != unary_functions.end()) {
            auto arg_eval = evaluate(func.args[0], ctx);

            // 1. Check for known symbolic values
            auto known_func = known_symbolic_unary.find(name);
            if (known_func != known_symbolic_unary.end()) {
                std::string key = expr_to_key(arg_eval);
                auto val_it = known_func->second.find(key);
                if (val_it != known_func->second.end()) {
                    return make_expr<Number>(val_it->second);
                }
            }

            // 2. Numeric evaluation
            if (std::holds_alternative<Number>(*arg_eval)) {
                double arg = get_number_value(arg_eval);
                return make_expr<Number>(it->second(arg));
            }

            // 3. Fallback: symbolic
            return make_fcall(name, { arg_eval });
        }
    }

    // 6. Built-in binary (with elementwise support)
    if (nargs == 2) {
        auto it = binary_functions.find(name);
        if (it != binary_functions.end()) {
            auto left = evaluate(func.args[0], ctx);
            auto right = evaluate(func.args[1], ctx);
            if (auto ew = elementwise(name, left, right)) return ew;
            if (std::holds_alternative<Number>(*left) && std::holds_alternative<Number>(*right)) {
                double a = get_number_value(left);
                double b = get_number_value(right);
                return make_expr<Number>(it->second(a, b));
            }
            return make_fcall(name, { left, right });
        }
        // Comparison functions
        auto cmp = comparison_functions.find(name);
        if (cmp != comparison_functions.end()) {
            double arg1 = get_number_value(evaluate(func.args[0], ctx));
            double arg2 = get_number_value(evaluate(func.args[1], ctx));
            return make_expr<Boolean>(cmp->second(arg1, arg2));
        }
    }

    // 7. Special n-ary handling for Plus/Times (symbolic math)
    if ((name == "Plus" || name == "Times") && nargs > 2) {
        double result = (name == "Plus") ? 0 : 1;
        bool all_numbers = true;
        std::vector<ExprPtr> simplified;
        for (const auto& arg : func.args) {
            auto evaluated_arg = evaluate(arg, ctx);
            if (std::holds_alternative<Number>(*evaluated_arg)) {
                double val = get_number_value(evaluated_arg);
                if (name == "Times" && val == 0) return make_expr<Number>(0);
                if ((name == "Plus" && val == 0) || (name == "Times" && val == 1)) continue;
                result = (name == "Plus") ? result + val : result * val;
            }
            else {
                all_numbers = false;
                simplified.push_back(evaluated_arg);
            }
        }
        if (all_numbers) return make_expr<Number>(result);
        if ((name == "Plus" && result != 0) || (name == "Times" && result != 1)) {
            simplified.insert(simplified.begin(), make_expr<Number>(result));
        }
        if (simplified.empty()) return make_expr<Number>((name == "Plus") ? 0 : 1);
        if (simplified.size() == 1) return simplified[0];
        return make_expr<FunctionCall>(name, simplified);
    }

    // 8. User-defined functions
    auto user_it = ctx.user_functions.find(name);
    if (user_it != ctx.user_functions.end()) {
        const FunctionDefinition& def = user_it->second;
        size_t param_count = def.params.size();
        size_t arg_count = func.args.size();
        std::vector<ExprPtr> final_args;
        for (size_t i = 0; i < param_count; ++i) {
            if (i < arg_count) {
                final_args.push_back(func.args[i]);
            }
            else if (def.params[i].default_value != nullptr) {
                final_args.push_back(def.params[i].default_value);
            }
            else {
                throw std::runtime_error("Function " + name + " expects at least " +
                    std::to_string(i + 1) + " arguments, got " +
                    std::to_string(arg_count));
            }
        }
        if (arg_count > param_count) {
            throw std::runtime_error("Function " + name + " expects at most " +
                std::to_string(param_count) + " arguments, got " +
                std::to_string(arg_count));
        }
        EvaluationContext local_ctx = ctx;
        for (size_t i = 0; i < param_count; ++i) {
            local_ctx.variables[def.params[i].name] = final_args[i];
        }
        return evaluate(def.body, local_ctx);
    }

    // 9. Fallback: return unevaluated
    std::vector<ExprPtr> unevaluated_args;
    for (const auto& arg : func.args) {
        unevaluated_args.push_back(arg);
    }
    return make_expr<FunctionCall>(name, unevaluated_args);
}

inline ExprPtr evaluate(const ExprPtr& expr, EvaluationContext& ctx, 
    std::unordered_set<std::string>& visited) {
    return std::visit(overloaded{
        [](const Number& num) -> ExprPtr {
            return make_expr<Number>(num.value);
        },
        [](const Boolean& boolean) -> ExprPtr {
            return make_expr<Boolean>(boolean.value);
        },
        [](const String& str) -> ExprPtr {
            return make_expr<String>(str.value);
        },
        [&](const Symbol& sym) -> ExprPtr {
            if (visited.count(sym.name)) {
                // Detected recursion, return symbol unevaluated
                return make_expr<Symbol>(sym.name);
            }
            auto it = ctx.variables.find(sym.name);
            if (it == ctx.variables.end()) {
                return make_expr<Symbol>(sym.name);
            }
            visited.insert(sym.name);
            auto result = evaluate(it->second, ctx, visited);
            visited.erase(sym.name);
            return result;
        },
        [&ctx](const FunctionCall& func) -> ExprPtr {
            // Special case: List
            if (func.head == "List") {
                std::vector<ExprPtr> evaluated_elements;
                for (const auto& arg : func.args) {
                    evaluated_elements.push_back(evaluate(arg, ctx));
                }
                return std::make_shared<Expr>(List{evaluated_elements});
            }

            // Check for user-defined functions
            auto user_it = ctx.user_functions.find(func.head);
            if (user_it != ctx.user_functions.end()) {
                const FunctionDefinition& def = user_it->second;
                size_t param_count = def.params.size();
                size_t arg_count = func.args.size();

                // Prepare argument list, filling in defaults if needed
                std::vector<ExprPtr> final_args;
                for (size_t i = 0; i < param_count; ++i) {
                    if (i < arg_count) {
                        final_args.push_back(func.args[i]);
                    }
                    else if (def.params[i].default_value != nullptr) {
                        final_args.push_back(def.params[i].default_value);
                    }
                    else {
                        throw std::runtime_error("Function " + func.head + " expects at least " +
                            std::to_string(i + 1) + " arguments, got " +
                            std::to_string(arg_count));
                    }
                }

                // Too many arguments
                if (arg_count > param_count) {
                    throw std::runtime_error("Function " + func.head + " expects at most " +
                        std::to_string(param_count) + " arguments, got " +
                        std::to_string(arg_count));
                }

                // Create a new context for the function evaluation
                EvaluationContext local_ctx = ctx;

                // Bind formal parameters to actual arguments
                for (size_t i = 0; i < param_count; ++i) {
                    auto arg_value = evaluate(final_args[i], ctx);
                    local_ctx.variables[def.params[i].name] = arg_value;
                }

                // Evaluate the function body in the new context
                return evaluate(def.body, local_ctx);
            }
            // If not a user-defined function, evaluate as a built-in function
            return evaluate_function(func, ctx);
        },
        [&ctx](const FunctionDefinition& def) -> ExprPtr {

            // Store the function definition in the context
            if (def.delayed) {
                // Delayed assignment: store the unevaluated body
                ctx.user_functions[def.name] = def;
            }
            else {
                // Immediate assignment: evaluate the body immediately
                EvaluationContext local_ctx = ctx;
                for (const auto& param : def.params) {
                    local_ctx.variables[param.name] = make_expr<Symbol>(param.name); // Bind parameters symbolically
                }
                auto evaluated_body = evaluate(def.body, local_ctx);
                ctx.user_functions[def.name] = FunctionDefinition(def.name, def.params, evaluated_body, false);
                return evaluated_body;
            }
            // Return the full function definition as feedback
            return make_expr<FunctionDefinition>(def.name, def.params, def.body, def.delayed);
        },
        [&ctx](const Assignment& assign) -> ExprPtr {
            // Evaluate the assigned value and store it in the context
            ctx.variables[assign.name] = evaluate(assign.value, ctx);

            // Return the variable name as feedback
            return make_expr<Symbol>(assign.name);
        },
        [&](const Rule& rule) -> ExprPtr {
            auto lhs = evaluate(rule.lhs, ctx, visited);
            auto rhs = evaluate(rule.rhs, ctx, visited);
            return make_expr<Rule>(lhs, rhs);
        },
        [](const List& list) -> ExprPtr {
            // Lists are already evaluated, just return as-is
            return std::make_shared<Expr>(list);
        },
        }, 
        *expr);
}

inline ExprPtr evaluate(const ExprPtr& expr, EvaluationContext& ctx) {
    std::unordered_set<std::string> visited;
    return evaluate(expr, ctx, visited);
}

}
