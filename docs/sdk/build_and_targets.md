# Build And Targets

The build now distinguishes the symbolic engine from the primary SDK path so
the new engine can compile without pulling in the broader symbolic parser and
evaluator internals.

Status note:

- this split is transitional
- the target architecture is one kernel plus SDK and pack layers above it
- the current SDK runtime path should not become a permanent peer semantic core

Related documents:

- [Kernel Refactor Urgent Plan](../kernel_refactor_urgent_plan.md)
- [Kernel Refactor Backlog](../kernel_refactor_backlog.md)
- [Kernel Representation Decision](../kernel_representation_decision.md)
- [Layer Ownership Matrix](../layer_ownership_matrix.md)

## Targets

| Target | Type | Purpose |
| --- | --- | --- |
| `aleph3_symbolic` | library | Current symbolic engine surface, expected to evolve toward an explicit kernel target |
| `aleph3_runtime` | interface library | Transitional runtime-facing include boundary placeholder |
| `aleph3_sdk` | library | Public SDK facade that should converge on kernel-backed execution |
| `aleph3_cli` | executable | Thin SDK tooling CLI for manual parser/validator/runtime checks |
| `aleph3_sdk_example` | executable | Minimal host-app example using registered demo host functions |
| `aleph3_symbolic_tests` | executable | Symbolic engine tests |
| `aleph3_sdk_tests` | executable | Rewrite SDK and IR tests |

## Build Options

- `ALEPH3_BUILD_SYMBOLIC_ENGINE=ON|OFF`
- `ALEPH3_BUILD_SDK=ON|OFF`
- `BUILD_TESTING=ON|OFF`

## Target Dependency Diagram

```mermaid
flowchart TD
    Symbolic["aleph3_symbolic"] --> SymbolicTests["aleph3_symbolic_tests"]
    Runtime["aleph3_runtime"] --> Sdk["aleph3_sdk"]
    Sdk --> SdkTests["aleph3_sdk_tests"]
    Sdk --> Cli["aleph3_cli"]
    Sdk --> Example["aleph3_sdk_example"]
```

This diagram reflects the current build, not the desired end state.
The desired end state is an explicit `aleph3_kernel` below the SDK and pack
targets.

## Practical Guidance

- Use `ALEPH3_BUILD_SDK=ON` to work on the primary SDK path.
- Treat the current SDK runtime evaluator as transitional while the repo moves
  toward one kernel.
- Use `aleph3_cli` for fast manual checks while broader validation and custom host-function tooling are still under construction.
- `validate` in the CLI now exercises the real lexer/parser/validator path.
- `evaluate` in the CLI now accepts `--var name=value` bindings for basic runtime checks.
- `evaluate-host` in the CLI registers demo host functions for end-to-end SDK checks.
- `aleph3_sdk_example` is the smallest compiled host-app integration reference in the repo.
- Use `ALEPH3_BUILD_SYMBOLIC_ENGINE=ON` when working on the symbolic engine core.
- Use `BUILD_TESTING=OFF` for offline or dependency-restricted compile checks.
- Keep new SDK components linked only through SDK targets unless a kernel
  dependency is explicitly justified.
- Do not add new permanent semantics to the current `runtime` layer without a
  kernel migration plan.
