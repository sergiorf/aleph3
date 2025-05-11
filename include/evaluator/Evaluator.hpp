// evaluator/Evaluator.hpp
#pragma once

#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"
#include "util/Overloaded.hpp"
#include "expr/ExprUtils.hpp"
#include "transforms/Transforms.hpp"

#include <stdexcept>
#include <cmath>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <unordered_set>

namespace mathix {

ExprPtr evaluate(const ExprPtr& expr, EvaluationContext& ctx);

inline ExprPtr evaluate_function(const FunctionCall& func, EvaluationContext& ctx) {
    //std::cout << "Evaluating function: " << func.head << std::endl;

    static const std::unordered_map<std::string, std::function<double(double)>> unary_functions = {
        {"sin", [](double x) { return std::sin(x); }},
        {"cos", [](double x) { return std::cos(x); }},
        {"tan", [](double x) { return std::tan(x); }},
        {"abs", [](double x) { return std::fabs(x); }},
        {"sqrt", [](double x) { return std::sqrt(x); }},
        {"log", [](double x) { return std::log(x); }},
        {"exp", [](double x) { return std::exp(x); }},
        {"floor", [](double x) { return std::floor(x); }},
        {"ceil", [](double x) { return std::ceil(x); }},
        {"round", [](double x) { return std::round(x); }}
    };

    static const std::unordered_map<std::string, std::function<double(double, double)>> binary_functions = {
        {"Plus", [](double a, double b) { return a + b; }},
        {"Minus", [](double a, double b) { return a - b; }},
        {"Times", [](double a, double b) { return a * b; }},
        {"Divide", [](double a, double b) { return a / b; }},
        {"Pow", [](double a, double b) { return std::pow(a, b); }}
    };

    static const std::unordered_map<std::string, std::function<bool(double, double)>> comparison_functions = {
        {"Equal", [](double a, double b) { return a == b; }},
        {"NotEqual", [](double a, double b) { return a != b; }},
        {"Less", [](double a, double b) { return a < b; }},
        {"Greater", [](double a, double b) { return a > b; }},
        {"LessEqual", [](double a, double b) { return a <= b; }},
        {"GreaterEqual", [](double a, double b) { return a >= b; }}
    };

    const std::string& name = func.head;

    // === Logical Operators ===
    if (name == "And") {
        for (const auto& arg : func.args) {
            auto evaluated_arg = evaluate(arg, ctx);

            // If the argument is Boolean and False, short-circuit
            if (std::holds_alternative<Boolean>(*evaluated_arg)) {
                if (!std::get<Boolean>(*evaluated_arg).value) {
                    return make_expr<Boolean>(false); // Short-circuit if any argument is False
                }
            }
            else {
                // If the argument is not Boolean, return unevaluated
                return make_expr<FunctionCall>("And", func.args);
            }
        }
        return make_expr<Boolean>(true); // All arguments are True
    }

    if (name == "Or") {
        for (const auto& arg : func.args) {
            auto evaluated_arg = evaluate(arg, ctx);

            // If the argument is Boolean and True, short-circuit
            if (std::holds_alternative<Boolean>(*evaluated_arg)) {
                if (std::get<Boolean>(*evaluated_arg).value) {
                    return make_expr<Boolean>(true); // Short-circuit if any argument is True
                }
            }
            else {
                // If the argument is not Boolean, return unevaluated
                return make_expr<FunctionCall>("Or", func.args);
            }
        }
        return make_expr<Boolean>(false); // All arguments are False
    }

    // === Handle If ===
    if (name == "If") {
       //std::cout << "Handling If statement" << std::endl;

        if (func.args.size() != 3) {
            throw std::runtime_error("If expects exactly 3 arguments");
        }

        // Evaluate the condition only
        auto condition = evaluate(func.args[0], ctx);

        // Check if the condition is Boolean
        if (std::holds_alternative<Boolean>(*condition)) {
            bool condition_value = std::get<Boolean>(*condition).value;
            //std::cout << "  Condition evaluated to: " << (condition_value ? "True" : "False") << std::endl;

            // Return the true branch if condition is true, otherwise the false branch
            return condition_value ? evaluate(func.args[1], ctx) : evaluate(func.args[2], ctx);
        }

        //std::cout << "  Condition is not Boolean, returning unevaluated If" << std::endl;
        // If the condition is not Boolean, return the If unevaluated
        return make_expr<FunctionCall>(name, func.args);
    }

    if (name == "Expand") {
        if (func.args.size() != 1) {
            throw std::runtime_error("Expand expects exactly one argument");
        }
        auto arg = evaluate(func.args[0], ctx);
        return expand(arg);
    }

    // === Unary Built-in ===
    if (auto it = unary_functions.find(name); it != unary_functions.end()) {
        if (func.args.size() != 1) {
            throw std::runtime_error(name + " expects 1 argument");
        }
        auto arg = evaluate(func.args[0], ctx);
        double value = get_number_value(arg);
        return make_expr<Number>(it->second(value));
    }

    // === Binary Built-in with Simplification ===
    if (name == "Plus" || name == "Times") {
        double result = (name == "Plus") ? 0 : 1;
        bool all_numbers = true;
        std::vector<ExprPtr> simplified;

        for (const auto& arg : func.args) {
            auto evaluated_arg = evaluate(arg, ctx);

            if (std::holds_alternative<Number>(*evaluated_arg)) {
                double val = get_number_value(evaluated_arg);

                // Early exit for Times: if any value is 0, whole product is 0
                if (name == "Times" && val == 0) {
                    return make_expr<Number>(0);
                }

                // Skip trivial values: 0 in Plus, 1 in Times
                if ((name == "Plus" && val == 0) || (name == "Times" && val == 1)) {
                    continue;
                }

                result = (name == "Plus") ? result + val : result * val;
            }
            else {
                all_numbers = false;
                simplified.push_back(evaluated_arg);
            }
        }

        if (all_numbers) {
            return make_expr<Number>(result);
        }

        // Add numeric part if not trivial
        if ((name == "Plus" && result != 0) || (name == "Times" && result != 1)) {
            simplified.insert(simplified.begin(), make_expr<Number>(result));
        }

        // Handle cases like Plus[] => 0 or Times[] => 1
        if (simplified.empty()) return make_expr<Number>((name == "Plus") ? 0 : 1);

        // Single argument: unwrap it (e.g., Plus[x] => x)
        if (simplified.size() == 1) return simplified[0];

        return make_expr<FunctionCall>(name, simplified);
    }

    if (name == "Minus" || name == "Divide" || name == "Pow") {
        if (func.args.size() != 2) {
            throw std::runtime_error(name + " expects 2 arguments");
        }

        // Lazily evaluate the left and right arguments
        auto left = evaluate(func.args[0], ctx);
        auto right = evaluate(func.args[1], ctx);

        // Handle fully numeric arguments
        if (std::holds_alternative<Number>(*left) && std::holds_alternative<Number>(*right)) {
            double a = get_number_value(left);
            double b = get_number_value(right);

            // Handle division by zero
            if (name == "Divide" && b == 0) {
                if (a == 0) {
                    return make_fcall("Indeterminate", {});
                }
                else if (a > 0) {
                    return make_fcall("DirectedInfinity", { make_expr<Number>(1) });
                }
                else {
                    return make_fcall("DirectedInfinity", { make_expr<Number>(-1) });
                }
            }

            return make_expr<Number>(binary_functions.at(name)(a, b));
        }

        // Trivial simplifications for exponentiation
        if (name == "Pow") {
            if (is_zero(right)) return make_expr<Number>(1); // x^0 = 1
            if (is_one(right)) return left;                 // x^1 = x
        }

        // Return symbolic form if not fully numeric
        return make_expr<FunctionCall>(name, std::vector{ left, right });
    }

    // Special case: unary negation
    if (name == "Negate") {
        if (func.args.size() != 1) {
            throw std::runtime_error("Negate expects exactly 1 argument");
        }

        // Lazily evaluate the argument
        auto arg = evaluate(func.args[0], ctx);

        // If the argument is a number, return the negated value
        if (std::holds_alternative<Number>(*arg)) {
            return make_expr<Number>(-get_number_value(arg));
        }

        // If the argument is symbolic, return a symbolic negation
        return make_expr<FunctionCall>("Times", std::vector{ make_expr<Number>(-1), arg });
    }

    // === Comparison Operators ===
    if (auto it = comparison_functions.find(name); it != comparison_functions.end()) {
        if (func.args.size() != 2) {
            throw std::runtime_error(name + " expects 2 arguments");
        }

        // Lazily evaluate the left and right arguments
        auto left = evaluate(func.args[0], ctx);
        auto right = evaluate(func.args[1], ctx);

        // Check if both arguments are numbers
        if (std::holds_alternative<Number>(*left) && std::holds_alternative<Number>(*right)) {
            double a = get_number_value(left);
            double b = get_number_value(right);
            bool result = it->second(a, b);
            return make_expr<Boolean>(result); // Return Boolean(true) or Boolean(false)
        }

        // If not both arguments are numbers, return the comparison unevaluated
        return make_expr<FunctionCall>(name, std::vector{ left, right });
    }

    // === User-defined Function ===
    if (auto user_it = ctx.user_functions.find(name); user_it != ctx.user_functions.end()) {
        const FunctionDefinition& def = user_it->second;

        if (def.params.size() != func.args.size()) {
            throw std::runtime_error("Function " + name + " expects " +
                std::to_string(def.params.size()) + " arguments, got " +
                std::to_string(func.args.size()));
        }

        // Create a new local context for the function evaluation
        EvaluationContext local_ctx = ctx;

        // Lazily bind formal parameters to actual arguments
        for (size_t i = 0; i < def.params.size(); ++i) {
            local_ctx.variables[def.params[i]] = func.args[i]; // Keep unevaluated for symbolic binding
        }

        // Evaluate the function body in the new context
        return evaluate(def.body, local_ctx);
    }

    // Unknown function: return unevaluated for symbolic preservation
    std::vector<ExprPtr> unevaluated_args;
    for (const auto& arg : func.args) {
        unevaluated_args.push_back(arg); // Keep arguments unevaluated
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
            // Check for user-defined functions
            auto user_it = ctx.user_functions.find(func.head);
            if (user_it != ctx.user_functions.end()) {
                const FunctionDefinition& def = user_it->second;

                if (def.params.size() != func.args.size()) {
                    throw std::runtime_error("Function " + func.head + " expects " +
                        std::to_string(def.params.size()) + " arguments, got " +
                        std::to_string(func.args.size()));
                }

                // Create a new context for the function evaluation
                EvaluationContext local_ctx = ctx;

                // Bind formal parameters to actual arguments
                for (size_t i = 0; i < def.params.size(); ++i) {
                    auto arg_value = evaluate(func.args[i], ctx);
                    local_ctx.variables[def.params[i]] = arg_value;
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
                    local_ctx.variables[param] = make_expr<Symbol>(param); // Bind parameters symbolically
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

            // Return the variable name as feedback (like Mathematica does)
            return make_expr<Symbol>(assign.name);
        }

        }, *expr);
}

inline ExprPtr evaluate(const ExprPtr& expr, EvaluationContext& ctx) {
    std::unordered_set<std::string> visited;
    return evaluate(expr, ctx, visited);
}

} // namespace mathix
