# Aleph Kernel Protocol M1 Plan

## Status

This is the implementation record for the first public/private split slice. It
details only M1 from
[Public App / Private Kernel Split](public_private_cli_split_plan.md): the
extraction-ready protocol model and framed JSON transport helpers.

Initial implementation is complete. The repository now has an independent
`aleph_client` target with protocol structs, JSON encode/decode helpers,
framing helpers, focused tests, and a boundary guard against private semantic
includes. This record remains useful until M2 replaces it as the active split
slice.

This plan is intentionally narrower than the repository split. It does not
implement `aleph-kernel`, move repositories, migrate the CLI, or create public
notebook UI code.

## Goal

Add a small `aleph_client` protocol component inside the current repository
that can later move to the public `aleph-notebook` or `aleph-client` codebase
without dragging private semantic implementation with it.

The slice succeeds when protocol messages can be encoded, decoded, framed, and
validated in tests without including kernel, parser, evaluator, session, SDK,
pack, or CLI headers.

## Roadmap Alignment

- Advances the public/private split direction in the unified plan.
- Supports the notebook-first product path by creating the future public app
  communication boundary before physical repository movement.
- Keeps the kernel as the only semantic core.
- Does not change mathematical behavior, CLI behavior, SDK behavior, notebook
  document behavior, or the supported symbolic subset.

## Ownership

The new component owns only transport-facing protocol data and framing:

```text
include/aleph_client/
src/aleph_client/
tests/aleph_client/
```

The exact paths may vary if the build layout suggests a better local pattern,
but the ownership rule is fixed: `aleph_client` must remain independent from
private semantic headers.

Allowed dependencies:

- C++ standard library;
- the repository-standard JSON dependency, if needed for JSON encode/decode;
- test framework in tests.

Forbidden dependencies:

- `expr`, `kernel`, `evaluator`, `symbols`, `normalizer`, `transforms`;
- `syntax`, `frontend`, parser internals, or `ir`;
- `session`;
- `sdk`;
- `packs`, algebra, calculus, or future domain implementation headers;
- `tooling` or CLI presentation code;
- web, notebook, or GUI code.

If a useful client field appears to require a forbidden dependency, treat that
as a protocol-design problem and add explicit transport data instead.

## Chosen Design

Use an explicit framed JSON protocol with JSON-RPC-like envelopes.

Initial framing:

```text
Content-Length: <byte-count>\r\n
\r\n
<json payload>
```

Initial protocol methods:

- `initialize`
- `version`
- `capabilities`
- `evaluate`
- `simplify`
- `fullForm`
- `help`
- `complete`
- `packages`
- `reset`
- `shutdown`

M1 does not need semantic handlers for those methods. It only needs typed
request/response data, JSON conversion, framing, validation, and tests.

Transport data should be plain and boring. Prefer strings, booleans, numbers
where stable, lists, maps, and small structs. Do not expose implementation
concepts such as `Expr`, syntax trees, evaluator contexts, exact scalar
storage, pack handler identities, or registry internals.

## Non-Goals

- starting or serving a real `aleph-kernel` process;
- launching child processes;
- timeouts, stderr capture, or process lifecycle;
- CLI migration;
- public-client-only build mode;
- notebook UI integration;
- semantic evaluation, simplification, completion, or help behavior;
- protocol support for parser tokens, parse trees, SDK IR, SDK compile,
  SDK host functions, or developer/debug commands.

## Implementation Tasks

### 1. Protocol Types

Implemented protocol-facing types for:

- request id;
- request envelope;
- success response envelope;
- error response envelope;
- protocol diagnostic;
- source span, if represented as transport data;
- evaluation result with source, ok status, representations, and diagnostics;
- initialize request and response;
- capabilities map or struct;
- help entry;
- completion entry;
- package entry.

Completion criteria:

- public headers compile without private includes;
- types are transport data only;
- response envelopes can represent both success and protocol/runtime errors.

Focused tests:

- construct an `evaluate` request;
- construct an initialize response with protocol version and capabilities;
- construct an evaluation result containing `text/plain` and diagnostics.

### 2. JSON Encoding And Decoding

Implemented encode/decode helpers for the initial envelope and payload shapes.

Completion criteria:

- encode an `evaluate` request with method, id, and params;
- decode an evaluation success response;
- decode an error response with stable code/message fields;
- preserve string result values exactly, including large exact-looking values;
- reject missing required envelope fields with a protocol diagnostic.

Focused tests:

- request id round trip;
- `evaluate` request JSON matches the expected field shape;
- success response with `representations["text/plain"] == "5/6"`;
- error response with code such as `kernel.division_by_zero`;
- malformed or incomplete envelope produces a deterministic failure.

### 3. Framed Message Reader And Writer

Implemented helpers for `Content-Length` framing.

Completion criteria:

- write a JSON payload with a correct byte length;
- read a complete frame from a stream or string-backed test harness;
- support payloads arriving independently of arbitrary read boundaries where
  the chosen abstraction exposes incremental reads;
- reject invalid headers, missing separator, negative lengths, non-numeric
  lengths, oversized lengths, and truncated payloads.

Focused tests:

- round trip one framed request;
- read two adjacent frames;
- reject invalid `Content-Length`;
- reject a body shorter than the declared length;
- reject a frame above the configured maximum.

### 4. Build Integration

Implemented build targets without changing existing behavior.

Targets:

```text
aleph_client
aleph_client_tests
```

Completion criteria:

- `aleph_client` builds independently of `aleph3_kernel` and `aleph3_sdk`;
- `aleph_client_tests` runs under CTest;
- existing kernel, SDK, CLI, notebook, and web targets do not depend on the new
  component yet;
- no existing executable behavior changes.

Focused verification:

```text
cmake --build build --config Release --target aleph_client_tests
ctest --test-dir build -C Release -R aleph_client_tests --output-on-failure
```

Adjust the configuration name or build directory to match the local generator.

### 5. Boundary Audit

Added an automated test guard that proves the new public protocol layer
does not include private semantic headers.

Current guard:

- focused test or script that scans `include/aleph_client` and
  `src/aleph_client` for forbidden include prefixes;
- plus code review of CMake target dependencies.

Completion criteria:

- forbidden includes fail the guard;
- target dependency graph keeps `aleph_client` below or beside product code,
  not above the kernel.

### 6. Documentation

Update only documents that own the new current behavior.

Completed documentation updates:

- `docs/public_private_cli_split_plan.md` marks M1 as initially implemented.
- `docs/architecture.md` records the protocol/client component in repository
  ownership and the build graph.
- `docs/sdk/build_and_targets.md` lists the new build target and tests.
- `docs/README.md` links to this implementation record.

Do not present `aleph-kernel`, CLI migration, or public notebook use as
shipped until later milestones implement them.

## Baseline And Verification

Before implementation:

- inspect current CMake target layout;
- inspect existing JSON dependency usage;
- inspect nearby test target conventions;
- run or record the nearest available baseline for build configuration.

After implementation:

```text
cmake --build build --config Release --target aleph_client_tests
ctest --test-dir build -C Release -R aleph_client_tests --output-on-failure
git diff --check
```

Run broader tests only if build target wiring or shared dependencies affect
existing targets. At minimum, review the final diff for forbidden dependencies,
duplicate semantics, accidental CLI behavior changes, and stale documentation.

## Completion Criteria

M1 is complete when:

- `aleph_client` protocol types and framed JSON helpers exist;
- focused protocol/framing tests pass;
- the component has no forbidden private semantic dependencies;
- existing user-visible behavior is unchanged;
- documentation names the protocol component as implemented without claiming a
  real kernel process or public repository split;
- the final diff passes whitespace checks and has been reviewed.

## Next Slice

After M1, proceed to M2: an in-tree private `aleph-kernel` executable that
owns a `session::Session` and serves the protocol methods over the framed JSON
transport.
