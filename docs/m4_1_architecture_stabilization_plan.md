# M4.1 Architecture Stabilization Plan

## Status

Proposed active milestone between the completed M4 private CLI runtime
migration and M5 private notebook-lite development.

This plan stabilizes Aleph3 around the runtime boundary before any M5 GUI or
notebook-lite product work begins. It is architecture work, not symbolic
mathematics expansion.

## Goal

M4.1 makes the runtime boundary extraction-ready by removing dependency cycles,
separating neutral protocol code from process-client code, and splitting the
notebook document model from private session and kernel implementation.

The intended direction is:

```text
Future public/product side

Notebook UI
Notebook document model
Agent UI
aleph_client
aleph_protocol
        |
        | framed JSON protocol
        v
--------------------------------
Private implementation side

aleph-runtime
Session
Kernel
Math packs
```

The dependency direction is one-way. Public or extraction-ready code must not
include private semantic headers.

## Non-Goals

M4.1 does not implement:

- Qt, desktop GUI, notebook-lite UI, or other M5 product surfaces;
- agent chat, LLM integration, or provider workflows;
- plotting, typesetting, streaming evaluation, cancellation, runtime restart,
  crash recovery, or remote/cloud runtime;
- new algebra, calculus, DSP, or broader symbolic algorithms;
- mathematical semantic changes except where required to preserve current
  behavior through the corrected architecture;
- repository physical split, licensing changes, or marketplace/plugin work.

Ordinary symbolic CLI behavior must continue to route through `aleph-runtime`.
Private developer tooling may remain direct where the architecture already
allows it, such as parser/token inspection, SDK validation, private inspection,
and host-function development commands.

## Architectural Invariants

- `aleph3_kernel` must not include pack headers or reference
  `register_*_pack` symbols.
- Packs depend on kernel registration contracts; the kernel never depends on
  packs.
- Protocol code must not depend on kernel, session, parser internals, exact
  arithmetic internals, packs, SDK semantic types, `Expr`, `ExprPtr`,
  `EvaluationContext`, or `FunctionRegistry`.
- Notebook model and persistence must not depend on session, kernel, parser,
  evaluator, packs, or SDK semantic types.
- Runtime-backed notebook execution must use the runtime client rather than
  constructing `session::Session` directly in extraction-safe model code.
- The kernel remains the only semantic core.
- Results persisted by notebooks are presentation caches, not semantic
  authority.

## Target Shape

Kernel and pack composition:

```text
Application composition
    |
    +-- Kernel core registry
    +-- Algebra pack registration
    +-- Calculus pack registration
    |
    v
Session
    |
    v
aleph-runtime
```

Protocol and client:

```text
aleph_protocol
      ^
      |
  +---+----------------+
  |                    |
aleph_client      RuntimeProtocolServer
```

Notebook:

```text
Notebook UI
    |
    v
aleph_notebook_model
    ^
    |
aleph_notebook_runtime
    |
    v
aleph_client
    |
    v
aleph_protocol
    |
    v
aleph-runtime
```

Likely targets:

```text
aleph_protocol
aleph_client
aleph3_kernel
aleph3_pack_algebra
aleph3_pack_calculus
aleph_notebook_model
aleph_notebook_runtime
aleph_runtime_protocol
aleph-runtime
```

Exact names may change to match repository conventions, but ownership and
dependency direction must remain clear.

## Baseline And Research

Before repo-tracked implementation edits, capture the current state:

1. Inspect current CMake targets and dependency edges for kernel, packs,
   session, runtime protocol, client, CLI, and notebook code.
2. Confirm the current Linux linker failure mode for `aleph-runtime`, including
   unresolved pack registration symbols.
3. Identify the repository's existing build options and decide whether an
   existing option can prove a kernel-disabled public subset build.
4. Run or record the closest practical baseline tests. Distinguish pre-existing
   failures from regressions introduced by M4.1.
5. Inspect the owning docs: `docs/architecture.md`,
   `docs/public_private_cli_split_plan.md`,
   `docs/aleph_runtime_protocol_m5_plan.md`,
   `docs/notebook_mvp_design.md`, and `README.md`.

Completion criterion: the implementation notes identify the current
dependency graph, build flags, affected targets, known failures, and docs that
must change.

## Workstream 1: Kernel And Pack Composition

### Problem

`src/evaluator/BuiltInFunctions.cpp` currently includes pack headers and calls:

```cpp
packs::register_algebra_pack(registry);
packs::register_calculus_pack(registry);
```

That creates:

```text
kernel -> packs -> kernel
```

Linux exposes the violation when linking `aleph-runtime` because pack
registration symbols remain unresolved.

### Desired Design

Kernel registration should create only the true core registry. Application or
runtime composition should create the default full registry:

```text
registry = create_core_registry()
register_algebra_pack(registry)
register_calculus_pack(registry)
Session(registry)
```

The composition API should be named and shared so full-engine consumers do not
each hand-roll different default pack sets. A name such as runtime bootstrap,
session factory, product registry factory, or application composition is
acceptable if it fits existing code.

### Implementation Tasks

1. Move pack registration out of the kernel builtin registration path.
2. Add or reuse a full-engine composition point that registers the current
   default algebra and calculus packs.
3. Route default runtime, CLI, SDK where appropriate, notebook runtime path,
   and tests through the full-engine composition path unless they explicitly
   request core-only behavior.
4. Add architecture regression coverage proving `aleph3_kernel` can build
   without pack implementation and does not link pack targets.
5. Preserve existing algebra and calculus behavior through `aleph-runtime`.
6. Update architecture docs for kernel/pack composition ownership.

Avoid fixing this through library-order tweaks, linker groups, repeated
libraries, platform-specific linker flags, folding packs into the kernel, or
making the kernel link against pack libraries.

## Workstream 2: Neutral Protocol Extraction

### Problem

`aleph_client` currently owns framing, protocol DTOs, and `RuntimeClient`.
The runtime server depends on protocol/framing types through the client target,
which makes the conceptual dependency direction:

```text
runtime server -> client
```

### Desired Design

Split neutral transport code from process-launching client code:

```text
aleph_protocol
    Framing
    Protocol

aleph_client
    RuntimeClient
    depends on aleph_protocol

aleph_runtime_protocol
    RuntimeProtocolServer
    depends on aleph_protocol
```

Suggested layout:

```text
include/aleph_protocol/
    Framing.hpp
    Protocol.hpp

src/aleph_protocol/
    Framing.cpp
    Protocol.cpp

include/aleph_client/
    RuntimeClient.hpp

src/aleph_client/
    RuntimeClient.cpp
```

Protocol DTOs should move toward a neutral namespace such as
`aleph3::protocol`. Compatibility aliases may be used only when they reduce
unrelated churn and must be documented as transitional.

### Implementation Tasks

1. Introduce the `aleph_protocol` target for framing, protocol data, and
   protocol JSON encoding/decoding.
2. Move or adapt DTOs into protocol-neutral ownership.
3. Make `aleph_client` depend on `aleph_protocol` and keep process launch,
   lifecycle, and request plumbing in the client target.
4. Make `RuntimeProtocolServer` depend on `aleph_protocol`, not conceptually
   on client-owned protocol types.
5. Keep protocol tests and framing tests independent of kernel/session/packs.
6. Keep fake runtime tests working through `aleph_client`.
7. Update architecture and public/private split docs for the target graph.

Completion criterion: `aleph_protocol` builds without private semantic targets,
and both fake and real runtime protocol tests use the neutral DTOs.

## Workstream 3: Session Symbol Inspection

### Goal

Add a read-only protocol operation for UI inspection of session-local symbols
without exposing private session, kernel, parser, or expression structures.

Preferred method name:

```text
sessionSymbols
```

Initial transport shape:

```cpp
struct SessionSymbolEntry {
    std::string name;
    std::string kind;
    std::string preview;
};
```

Initial `kind` values should be stable, small, and mapped from current session
semantics. Likely values:

```text
value
function
delayed_function
other
```

The first version exposes session-local entries only. Builtins and pack
functions remain help/discovery concerns unless a later design expands this
operation.

### Implementation Tasks

1. Add protocol DTOs and JSON encode/decode for the request and response.
2. Add `RuntimeProtocolServer` support by querying session-owned state and
   rendering text previews without exposing `Expr`.
3. Add `RuntimeClient` support.
4. Add fake runtime tests for the protocol/client surface.
5. Add real runtime protocol tests:

   ```text
   a = 2
   f[x_] := x + a
   sessionSymbols contains a and f
   Clear[a]
   sessionSymbols updates
   reset
   session-local entries disappear
   ```

6. Document the operation as read-only session inspection, not mutation or
   semantic authority.

Completion criterion: notebook or UI code can inspect session-local symbols
through the protocol without private headers or result parsing.

## Workstream 4: Notebook Model And Runtime Split

### Problem

The current notebook core includes session headers, stores
`session::SessionDiagnostic`, and directly constructs `session::Session` in
`Runner::run_all`. That makes the document model a private semantic consumer.

### Desired Design

Split notebook ownership into two layers:

```text
aleph_notebook_model
    Cell
    CellKind
    Document
    GeneratedResult
    NotebookDiagnostic
    PersistenceLimits
    DocumentError
    JSON encode/decode
    load/save
    cached result lifecycle
    document validation

aleph_notebook_runtime
    runtime-backed Run All/orchestration
    depends on aleph_notebook_model
    depends on aleph_client
```

The model must not depend on `Session`, kernel, parser, evaluator, packs, SDK,
`Expr`, or private semantic headers.

Replace persisted or model-owned `session::SessionDiagnostic` with a notebook
owned or protocol-neutral representation, for example:

```cpp
struct NotebookDiagnostic {
    std::string code;
    std::string severity;
    std::string message;
    std::optional<SourceSpan> span;
};
```

Persistence should remain compatible with current notebook v1 files. If a
migration is unavoidable, it must be deterministic and tested.

### Runtime-Backed Run All Contract

`Run All` must preserve the existing semantic intent:

```text
Run All starts from a clean runtime session
cells execute in order
definitions flow between cells during that run
failed cells record diagnostics
later cells continue when safe
results are cached presentation data, not semantic authority
```

Opening a notebook must not execute it. Saving and reopening must not silently
evaluate cells.

### Implementation Tasks

1. Create the extraction-safe notebook model target and move model,
   validation, persistence, cached result, and notebook-owned diagnostic types
   into it.
2. Preserve notebook v1 load/save compatibility and cached result behavior.
3. Move semantic execution into a runtime-backed notebook runner target.
4. Make the runtime-backed runner use `aleph_client` and the protocol. It must
   not parse or evaluate symbolic expressions itself.
5. Remove or quarantine direct `session::Session` construction from
   extraction-safe notebook code.
6. Add model tests that build without kernel:

   ```text
   save/reopen does not evaluate
   cached results survive reopen
   clearing cached results leaves source intact
   malformed and oversized documents remain bounded
   ```

7. Add runtime runner tests:

   ```text
   definitions flow between cells
   Run All starts clean
   failed cell records diagnostics
   later cells continue
   runtime failure becomes notebook-level diagnostic
   no direct Session construction in extraction-safe notebook code
   ```

8. Update notebook MVP and architecture docs to explain that model/persistence
   is not a semantic evaluator.

Completion criterion: notebook model and persistence compile without private
semantic targets, while runtime-backed execution preserves current clean
`Run All` behavior through `aleph-runtime`.

## Workstream 5: Extraction Build Proof

Add a build or test configuration proving that the future public-facing subset
can compile without the private kernel implementation.

The proof must cover the equivalent of:

```text
aleph_protocol
aleph_client
aleph_notebook_model
fake runtime tests
```

The proof must exclude:

```text
aleph3_kernel
session implementation
aleph3_pack_algebra
aleph3_pack_calculus
private CLI
```

Use an existing CMake option if one can prove this boundary. Add a new option
only if the existing configuration cannot express it cleanly. A possible name
is `ALEPH3_BUILD_KERNEL=OFF`, but the exact flag must be chosen after CMake
research.

Completion criterion: CI or a focused local build mechanically proves that
protocol, client, notebook model, and fake runtime tests build without private
semantic implementation.

## Workstream 6: CI, Formatting, And Documentation

### CI Failures To Address

- Linux currently fails linking `aleph-runtime` because the kernel references
  algebra and calculus pack registration.
- clang-format currently reports violations in files including:

  ```text
  src/tooling/aleph3_cli.cpp
  tests/tooling/Aleph3CliTests.cpp
  ```

### Tasks

1. Fix the Linux linker failure through the kernel/pack composition change,
   not linker workarounds.
2. Run the repository's expected clang-format rules on changed C++ files
   required by M4.1.
3. Update docs as each vertical task changes ownership or behavior.
4. At minimum inspect and update:

   ```text
   README.md
   docs/architecture.md
   docs/public_private_cli_split_plan.md
   docs/aleph_runtime_protocol_m5_plan.md
   docs/notebook_mvp_design.md
   ```

5. Fix README wording that still implies the private CLI runtime migration is
   incomplete.
6. Verify local documentation links affected by the change.

## Recommended Implementation Order

Use this order unless repository research reveals a safer equivalent sequence:

1. Baseline dependency, CMake option, and test research.
2. Fix kernel/pack composition and restore Linux runtime linking.
3. Extract `aleph_protocol`.
4. Migrate `RuntimeClient` and `RuntimeProtocolServer` to the neutral protocol.
5. Add read-only `sessionSymbols` protocol support.
6. Split notebook model from runtime-backed notebook execution.
7. Add the extraction/kernel-disabled build proof.
8. Fix formatting and run final verification.
9. Update or finish docs for the final dependency graph and M4.1 status.

This order intentionally places `sessionSymbols` before notebook runtime work
because it belongs to the protocol/session/runtime boundary and can be tested
before the notebook runner consumes the cleaner client layer.

## Verification

Run the relevant full build and tests locally:

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Also run focused runtime, client, protocol, notebook model, notebook runtime,
and fake runtime tests.

Run the extraction build configuration that proves:

```text
aleph_protocol
aleph_client
aleph_notebook_model
fake runtime tests
```

do not depend on private semantic implementation.

Run:

```bash
git diff --check
```

Run the repository's clang-format verification equivalent on changed C++
files.

If a command cannot be run locally, record why and identify the closest
evidence that was obtained.

## Completion Criteria

M4.1 is complete only when:

1. `aleph3_kernel` no longer references algebra or calculus pack registration
   functions.
2. Kernel-to-pack dependency cycles are removed.
3. Linux `aleph-runtime` links without linker workarounds.
4. Windows behavior remains working.
5. `aleph_protocol` is a neutral extraction-safe transport layer.
6. `RuntimeClient` depends on `aleph_protocol`.
7. `RuntimeProtocolServer` uses `aleph_protocol` rather than client-owned
   protocol types.
8. Notebook model and persistence no longer include or depend on
   `session::Session`.
9. Notebook model and persistence compile without kernel/private semantic
   code.
10. Runtime-backed notebook execution uses `aleph_client`.
11. `Run All` preserves clean-session semantics.
12. A structured read-only `sessionSymbols` protocol operation exists and is
    covered by fake and real runtime tests.
13. Extraction-safe targets build with private kernel implementation disabled.
14. Ordinary symbolic CLI behavior continues through `aleph-runtime`.
15. Existing symbolic behavior remains compatible.
16. Focused and affected broader tests pass.
17. clang-format CI passes for changed C++ files.
18. `git diff --check` passes.
19. Documentation accurately reflects M4 completion and the M4.1 architecture.
20. No M5 GUI, agent, or new symbolic mathematics functionality has been
    implemented.

## Final Report Template

When finishing M4.1, report:

```text
Architecture changes
Files/targets added
Files/targets removed or renamed
Kernel/packs dependency fix
Protocol extraction summary
Notebook decoupling summary
sessionSymbols protocol shape
Extraction-build proof
Tests added/updated
Commands run
Test results
Any remaining technical debt
Recommended next step for M5
```

Also include the final target dependency graph in compact text form.

Do not claim success unless the relevant build and tests were actually run.
Clearly distinguish unrelated pre-existing failures from M4.1 regressions.
