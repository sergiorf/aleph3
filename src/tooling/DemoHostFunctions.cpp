#include "tooling/DemoHostFunctions.hpp"

#include <algorithm>
#include <array>
#include <span>

namespace aleph3::tooling {

namespace {

constexpr std::array<std::string_view, 3> kDemoFunctionNames = {
    "Clamp",
    "ScaleAdd",
    "PickLabel"
};

constexpr std::array<DemoHostFunctionDoc, 3> kDemoFunctionDocs = {{
    {
        "Clamp",
        "Clamp[value, low, high]",
        "Bounds a number to an inclusive range.",
        "aleph3_cli evaluate-host --var x=12 \"Clamp[x, 0, 10]\"",
        "evaluate-host --var x=12 Clamp[x, 0, 10]"
    },
    {
        "ScaleAdd",
        "ScaleAdd[value, scale, offset]",
        "Computes value * scale + offset.",
        "aleph3_cli evaluate-host --var x=4 \"ScaleAdd[x, 1.5, 2]\"",
        "evaluate-host --var x=4 ScaleAdd[x, 1.5, 2]"
    },
    {
        "PickLabel",
        "PickLabel[flag, whenTrue, whenFalse]",
        "Chooses one of two string labels from a boolean flag.",
        "aleph3_cli evaluate-host --var flag=True \"PickLabel[flag, \\\"ok\\\", \\\"fail\\\"]\"",
        "evaluate-host --var flag=True PickLabel[flag, \"ok\", \"fail\"]"
    }
}};

EvaluationResult success(Value value) {
    EvaluationResult result;
    result.value = std::move(value);
    return result;
}

}  // namespace

void register_demo_host_functions(Engine& engine, Schema& schema) {
    HostFunctionSpec clamp;
    clamp.name = "Clamp";
    clamp.arity = FunctionArity::exact(3);
    clamp.parameters = {
        {"value", ValueType::number, true},
        {"low", ValueType::number, true},
        {"high", ValueType::number, true}
    };
    clamp.return_type = ValueType::number;
    clamp.description = "Clamp[value, low, high] bounds a number to an inclusive range.";
    clamp.callback = [](std::span<const Value> args) {
        const double value = *args[0].as_number();
        const double low = *args[1].as_number();
        const double high = *args[2].as_number();
        return success(Value(std::max(low, std::min(value, high))));
    };
    engine.register_function(clamp);
    schema.allow_function({
        clamp.name,
        clamp.arity,
        {ValueType::number, ValueType::number, ValueType::number},
        ValueType::number,
        true});

    HostFunctionSpec scale_add;
    scale_add.name = "ScaleAdd";
    scale_add.arity = FunctionArity::exact(3);
    scale_add.parameters = {
        {"value", ValueType::number, true},
        {"scale", ValueType::number, true},
        {"offset", ValueType::number, true}
    };
    scale_add.return_type = ValueType::number;
    scale_add.description = "ScaleAdd[value, scale, offset] computes value * scale + offset.";
    scale_add.callback = [](std::span<const Value> args) {
        const double value = *args[0].as_number();
        const double scale = *args[1].as_number();
        const double offset = *args[2].as_number();
        return success(Value(value * scale + offset));
    };
    engine.register_function(scale_add);
    schema.allow_function({
        scale_add.name,
        scale_add.arity,
        {ValueType::number, ValueType::number, ValueType::number},
        ValueType::number,
        true});

    HostFunctionSpec pick_label;
    pick_label.name = "PickLabel";
    pick_label.arity = FunctionArity::exact(3);
    pick_label.parameters = {
        {"flag", ValueType::boolean, true},
        {"when_true", ValueType::string, true},
        {"when_false", ValueType::string, true}
    };
    pick_label.return_type = ValueType::string;
    pick_label.description = "PickLabel[flag, whenTrue, whenFalse] chooses a string label.";
    pick_label.callback = [](std::span<const Value> args) {
        const bool flag = *args[0].as_boolean();
        return success(Value(flag ? *args[1].as_string() : *args[2].as_string()));
    };
    engine.register_function(pick_label);
    schema.allow_function({
        pick_label.name,
        pick_label.arity,
        {ValueType::boolean, ValueType::string, ValueType::string},
        ValueType::string,
        true});
}

std::span<const std::string_view> demo_host_function_names() noexcept {
    return kDemoFunctionNames;
}

std::span<const DemoHostFunctionDoc> demo_host_function_docs() noexcept {
    return kDemoFunctionDocs;
}

}  // namespace aleph3::tooling
