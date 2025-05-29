#include "evaluator/SimplificationRules.hpp"
#include "expr/ExprUtils.hpp"
#include <cmath>

namespace aleph3 {
    const std::unordered_map<std::string, SimplifyRule> simplification_rules = {
    {"Plus", [](const std::vector<ExprPtr>& args, EvaluationContext& ctx,
            const std::function<ExprPtr(const ExprPtr&, EvaluationContext&)>& eval) -> ExprPtr {
        std::vector<ExprPtr> eval_args;
        for (const auto& arg : args) {
            eval_args.push_back(eval(arg, ctx));
        }

        // Elementwise list support for two lists
        if (eval_args.size() == 2 &&
            std::holds_alternative<List>(*eval_args[0]) &&
            std::holds_alternative<List>(*eval_args[1])) {
            const auto& l1 = std::get<List>(*eval_args[0]).elements;
            const auto& l2 = std::get<List>(*eval_args[1]).elements;
            if (l1.size() != l2.size())
                throw std::runtime_error("List sizes must match for elementwise Plus");
            std::vector<ExprPtr> result;
            for (size_t i = 0; i < l1.size(); ++i) {
                result.push_back(eval(make_fcall("Plus", { l1[i], l2[i] }), ctx));
            }
            return std::make_shared<Expr>(List{ result });
        }

        // Scalar and list broadcasting (optional)
        if (eval_args.size() == 2) {
            if (std::holds_alternative<List>(*eval_args[0]) && std::holds_alternative<Number>(*eval_args[1])) {
                const auto& l1 = std::get<List>(*eval_args[0]).elements;
                std::vector<ExprPtr> result;
                for (const auto& elem : l1) {
                    result.push_back(eval(make_fcall("Plus", { elem, eval_args[1] }), ctx));
                }
                return std::make_shared<Expr>(List{ result });
            }
            if (std::holds_alternative<Number>(*eval_args[0]) && std::holds_alternative<List>(*eval_args[1])) {
                const auto& l2 = std::get<List>(*eval_args[1]).elements;
                std::vector<ExprPtr> result;
                for (const auto& elem : l2) {
                    result.push_back(eval(make_fcall("Plus", { eval_args[0], elem }), ctx));
                }
                return std::make_shared<Expr>(List{ result });
            }
        }

        // Original Plus rule for scalars and mixed
        double result = 0;
        std::vector<ExprPtr> simplified;
        for (const auto& e : eval_args) {
            if (std::holds_alternative<Number>(*e)) {
                double val = get_number_value(e);
                if (val == 0) continue;
                result += val;
            }
            else {
                simplified.push_back(e);
            }
        }
        if (result != 0) simplified.insert(simplified.begin(), make_expr<Number>(result));
        if (simplified.empty()) return make_expr<Number>(0);
        if (simplified.size() == 1) return simplified[0];
        return make_fcall("Plus", simplified);
    }},
    {"Times", [](const std::vector<ExprPtr>& args, EvaluationContext& ctx,
             const std::function<ExprPtr(const ExprPtr&, EvaluationContext&)>& eval) -> ExprPtr {
        double result = 1;
        std::vector<ExprPtr> simplified;
        for (const auto& arg : args) {
            auto e = eval(arg, ctx);
            if (std::holds_alternative<Number>(*e)) {
                double val = get_number_value(e);
                if (val == 0) return make_expr<Number>(0);
                if (val == 1) continue;
                result *= val;
            }
            else {
                simplified.push_back(e);
            }
        }
        if (result != 1) simplified.insert(simplified.begin(), make_expr<Number>(result));
        if (simplified.empty()) return make_expr<Number>(1);
        if (simplified.size() == 1) return simplified[0];
        return make_fcall("Times", simplified);
    }},
    {"Power", [](const std::vector<ExprPtr>& args, EvaluationContext& ctx,
             const std::function<ExprPtr(const ExprPtr&, EvaluationContext&)>& eval) -> ExprPtr {
        if (args.size() != 2) return make_fcall("Power", args);
        auto base = eval(args[0], ctx);
        auto exp = eval(args[1], ctx);
        if (std::holds_alternative<Number>(*exp)) {
            double e = get_number_value(exp);
            if (e == 0) return make_expr<Number>(1.0);
            if (e == 1) return base;
        }
        if (std::holds_alternative<Number>(*base)) {
            double b = get_number_value(base);
            if (b == 0) return make_expr<Number>(0.0);
            if (b == 1) return make_expr<Number>(1.0);
        }
        if (std::holds_alternative<Number>(*base) && std::holds_alternative<Number>(*exp)) {
            double b = get_number_value(base);
            double e = get_number_value(exp);
            return make_expr<Number>(std::pow(b, e));
        }
        return make_fcall("Power", {base, exp});
    }},
    {"Divide", [](const std::vector<ExprPtr>& args, EvaluationContext& ctx,
              const std::function<ExprPtr(const ExprPtr&, EvaluationContext&)>& eval) -> ExprPtr {
        if (args.size() != 2) return make_fcall("Divide", args);
            auto num = eval(args[0], ctx);
            auto denom = eval(args[1], ctx);
            if (std::holds_alternative<Number>(*num) && std::holds_alternative<Number>(*denom)) {
                double a = get_number_value(num);
                double b = get_number_value(denom);
                if (b == 0.0) {
                    if (a == 0.0) return make_expr(Indeterminate{});
                    // TODO: Return Infinity or ComplexInfinity for a != 0
            }
            return make_expr<Number>(a / b);
        }
        return make_fcall("Divide", {num, denom});
    }},
    };
}