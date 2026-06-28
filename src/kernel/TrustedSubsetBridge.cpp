#include "kernel/TrustedSubsetBridge.hpp"

#include "evaluator/BuiltInFunctions.hpp"
#include "evaluator/Evaluator.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "kernel/Diagnostics.hpp"
#include "kernel/EvaluationContext.hpp"

#include <cmath>
#include <optional>
#include <stdexcept>
#include <utility>

namespace aleph3::kernel {

namespace {

std::optional<Value> expr_to_sdk_value(const ExprPtr& expr) {
    if (expr == nullptr) {
        return std::nullopt;
    }

    if (const auto* number = std::get_if<Number>(&*expr)) {
        double value = number->value;
        if (value == 0.0) {
            value = 0.0;
        }
        return Value(value);
    }
    if (const auto* rational = std::get_if<Rational>(&*expr)) {
        return Value(static_cast<double>(rational->numerator) / rational->denominator);
    }
    if (const auto* boolean = std::get_if<Boolean>(&*expr)) {
        return Value(boolean->value);
    }
    if (const auto* string = std::get_if<String>(&*expr)) {
        return Value(string->value);
    }
    if (const auto* list = std::get_if<List>(&*expr)) {
        Value::List values;
        values.reserve(list->elements.size());
        for (const auto& element : list->elements) {
            auto converted = expr_to_sdk_value(element);
            if (!converted.has_value()) {
                return std::nullopt;
            }
            values.push_back(std::move(*converted));
        }
        return Value(std::move(values));
    }

    return std::nullopt;
}

ExprPtr sdk_value_to_expr(const Value& value) {
    if (const auto* number = value.as_number()) {
        return make_expr<Number>(*number);
    }
    if (const auto* boolean = value.as_boolean()) {
        return make_expr<Boolean>(*boolean);
    }
    if (const auto* string = value.as_string()) {
        return make_expr<String>(*string);
    }
    if (const auto* list = value.as_list()) {
        std::vector<ExprPtr> elements;
        elements.reserve(list->size());
        for (const auto& element : *list) {
            elements.push_back(sdk_value_to_expr(element));
        }
        return std::make_shared<Expr>(List{elements});
    }
    return make_expr<Indeterminate>();
}

void seed_kernel_symbols(
    EvaluationContext& ctx,
    const Bindings& constants,
    const Bindings& bindings) {
    for (const auto& [name, value] : constants) {
        ctx.symbol_values.set(name, sdk_value_to_expr(value));
    }
    for (const auto& [name, value] : bindings) {
        ctx.symbol_values.set(name, sdk_value_to_expr(value));
    }
}

}  // namespace

StagedTrustedSubsetFormula stage_trusted_subset_formula(const ir::NodePtr& root) {
    StagedTrustedSubsetFormula staged;

    // Lower validated trusted-subset IR directly into the kernel execution
    // form. The bridge no longer preserves IR as execution state.
    auto lowered = lower_trusted_ir_to_expr(root);
    staged.kernel_expr = std::move(lowered.expr);
    staged.diagnostics = std::move(lowered.diagnostics);
    return staged;
}

EvaluationResult evaluate_trusted_subset_formula(
    const ExprPtr& kernel_expr,
    const Bindings& bindings,
    const Bindings& constants,
    const HostFunctionRegistry& host_functions,
    const FunctionRegistry& function_registry,
    const Policy& policy) {
    if (kernel_expr == nullptr) {
        EvaluationResult result;
        result.error = make_runtime_error(
            ErrorCode::internal_inconsistency,
            "Compiled formula is missing its lowered kernel expression.");
        return result;
    }

    try {
        EvaluationContext ctx(bindings, constants, host_functions, policy, function_registry);
        ctx.enable_runtime_strict_semantics(true);
        ctx.reset_runtime_step_counter();
        seed_kernel_symbols(ctx, constants, bindings);

        auto result_expr = evaluate(kernel_expr, ctx);
        auto value = expr_to_sdk_value(result_expr);
        if (value.has_value()) {
            EvaluationResult result;
            result.value = std::move(*value);
            return result;
        }

        return {};
    } catch (const RuntimeFailure& failure) {
        EvaluationResult result;
        result.error = failure.error();
        return result;
    } catch (const EvaluatorError& error) {
        EvaluationResult result;
        result.error = make_runtime_error(error.code(), error.what());
        return result;
    } catch (const std::runtime_error& error) {
        EvaluationResult result;
        result.error = make_runtime_error(ErrorCode::internal_inconsistency, error.what());
        return result;
    }
}

EvaluationResult evaluate_trusted_subset_formula(
    const ExprPtr& kernel_expr,
    const Bindings& bindings,
    const Bindings& constants,
    const HostFunctionRegistry& host_functions,
    const Policy& policy) {
    return evaluate_trusted_subset_formula(
        kernel_expr,
        bindings,
        constants,
        host_functions,
        default_function_registry(),
        policy);
}

}  // namespace aleph3::kernel
