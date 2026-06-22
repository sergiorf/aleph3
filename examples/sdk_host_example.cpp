#include "sdk/Engine.hpp"
#include "tooling/DemoHostFunctions.hpp"

#include <iostream>

int main() {
    aleph3::Engine engine;
    aleph3::Schema schema;
    aleph3::tooling::register_demo_host_functions(engine, schema);
    schema.allow_variable({"score", aleph3::ValueType::number, true});
    schema.allow_variable({"approved", aleph3::ValueType::boolean, true});

    const auto compile_result = engine.compile(
        R"(If[approved, PickLabel[True, "ok", "fail"], PickLabel[False, "ok", "fail"]])",
        schema);
    if (!compile_result.ok()) {
        for (const auto& diagnostic : compile_result.diagnostics) {
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        }
        return 2;
    }

    const aleph3::Bindings bindings = {
        {"score", aleph3::Value(12.0)},
        {"approved", aleph3::Value(true)}
    };

    const auto label_result = engine.evaluate(*compile_result.formula, bindings);
    if (!label_result.ok()) {
        std::cerr << label_result.error->code << ": " << label_result.error->message << '\n';
        return 2;
    }

    const auto numeric_formula = engine.compile("Clamp[ScaleAdd[score, 1.5, 2], 0, 20]", schema);
    if (!numeric_formula.ok()) {
        for (const auto& diagnostic : numeric_formula.diagnostics) {
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        }
        return 2;
    }

    const auto numeric_result = engine.evaluate(*numeric_formula.formula, bindings);
    if (!numeric_result.ok()) {
        std::cerr << numeric_result.error->code << ": " << numeric_result.error->message << '\n';
        return 2;
    }

    std::cout << "label=" << *label_result.value->as_string() << '\n';
    std::cout << "score=" << *numeric_result.value->as_number() << '\n';
    return 0;
}
