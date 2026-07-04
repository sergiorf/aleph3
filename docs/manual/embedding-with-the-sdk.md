# Embedding With The SDK

## The Host Boundary

The SDK exposes the kernel through host-controlled contracts:

- `Engine` compiles and evaluates formulas
- `Schema` declares names and types
- `Policy` enables operations and limits work
- host-facing values and diagnostics return results
- host-function registration adds application behavior with declared types

The SDK does not implement a second evaluator. Validated formulas are lowered
to the same kernel expression model used by symbolic tools.

## Compile Once, Evaluate With Bindings

A normal workflow constructs an engine, schema, and policy, compiles source,
checks diagnostics, then evaluates the opaque formula with different bindings:

```text
schema:  temperature is Number, limit is Number
formula: If[temperature > limit, "alarm", "ok"]

temperature = 24, limit = 20  -> "alarm"
temperature = 18, limit = 20  -> "ok"
```

Names missing from the schema are rejected before runtime. The broader symbolic
surface may instead preserve an unknown name.

The following is the minimal shape of the public C++ workflow:

```cpp
#include "sdk/Engine.hpp"

int main() {
    aleph3::Engine engine;
    aleph3::Schema schema;
    schema.allow_variable({"x", aleph3::ValueType::number, true});

    const auto compiled = engine.compile("x + 1", schema);
    if (!compiled.ok()) {
        return 2; // Display compiled.diagnostics in a real host.
    }

    const aleph3::Bindings bindings = {{"x", aleph3::Value(3.0)}};
    const auto result = engine.evaluate(*compiled.formula, bindings);
    if (!result.ok()) {
        return 2; // Display result.error in a real host.
    }

    const double answer = *result.value->as_number(); // 4.0
    return answer == 4.0 ? 0 : 1;
}
```

Production code must handle both compilation diagnostics and the runtime error
returned by evaluation. The complete, build-tested example is
[`examples/sdk_host_example.cpp`](../../examples/sdk_host_example.cpp).

## Policies And Budgets

A policy controls which language features the host accepts and how much work a
formula may consume. Validation catches disallowed calls, type mismatches,
unknown variables, excessive depth, and selected constant runtime traps.
Runtime checks still matter because bindings and callbacks can fail in ways
source validation cannot predict.

The SDK trusted subset is intentionally narrower than the symbolic session. It
does not expose assignments, rewrite rules, user definitions, polynomial
commands, or symbolic fallback as host-visible runtime values.

## Host Functions

A host function declares its name, argument types, return type, and callback:

```text
host registers: PriceForSku[String] -> Number
formula:        PriceForSku[sku] * quantity
```

Registration is engine-scoped. Aleph3 validates calls and checks callback
results at runtime. The CLI demo bundle provides examples:

```text
:host-functions
:evaluate-host --var x=12 Clamp[x, 0, 10]
:evaluate-host --var x=4 ScaleAdd[x, 1.5, 2]
```

## Diagnostics

Compilation returns structured diagnostics with stable codes and source spans
where available. Evaluation returns a value or structured runtime error. Hosts
should use codes for behavior and messages for display, not parse prose.

## Detailed References

- [SDK documentation](../sdk/README.md)
- [Stable interfaces](../sdk/stable_interfaces.md)
- [Trusted subset](../trusted_subset_v1.md)
- [Build and targets](../sdk/build_and_targets.md)
