# Aleph Runtime Protocol M2 Plan

## Status

M2 is implemented. This document is the archived implementation record for
the in-tree private `aleph-runtime` executable from
[Public App / Private Kernel Split](../public_private_cli_split_plan.md).
Current build and usage facts live in the architecture, manual, and build
target documentation.

M1 is complete and archived in
[Aleph Runtime Protocol M1 Plan](aleph_runtime_protocol_m1_plan.md). The
current repository now has protocol data and framing helpers in `aleph_client`
and the real private `aleph-runtime` process. Client launch behavior, CLI
migration, and the physical repository split remain later work.

## Goal

Add a private `aleph-runtime` executable inside the current repository. The
process reads `Content-Length` framed JSON requests from standard input, maps
supported protocol methods to the shared session/kernel behavior, writes framed
JSON responses to standard output, and reports protocol/runtime failures using
transport data only.

`aleph-runtime` is the private computation process boundary, not the
domain-independent kernel core. It may host the kernel core, session state, and
registered private packs. The kernel core remains domain-independent, and
domain algorithms stay in registered packs rather than being folded into
kernel-core implementation.

The slice succeeds when a process-level test can start the executable, send
framed requests for initialization, evaluation, simplification, full form,
help, completion, package discovery, reset, version, capabilities, and
shutdown, and observe deterministic responses without exposing private
semantic types through the protocol.

## Roadmap Alignment

- Advances the public/private split direction in the unified plan and
  [Public App / Private Kernel Split](../public_private_cli_split_plan.md).
- Supports the notebook-first product path by giving future public notebook
  and app-client code a real private runtime process to speak to.
- Keeps the kernel and session as the only semantic core; the executable is a
  transport adapter, not a second evaluator.
- Does not migrate the existing CLI, create a public client process launcher,
  move repositories, or change the supported symbolic subset.

## Ownership

The new executable and any private server adapter belong with private tooling
and session-facing implementation, not with `aleph_client`:

```text
src/tooling/aleph_kernel.cpp
src/tooling/RuntimeProtocolServer.cpp
include/tooling/RuntimeProtocolServer.hpp
tests/tooling/AlephRuntimeProtocolTests.cpp
```

Exact file names may vary to fit the local build, but dependency direction is
fixed:

- `aleph-runtime` may link private semantic targets: `aleph3_kernel`,
  `aleph3_pack_algebra`, `aleph3_pack_calculus`, `aleph3_syntax`,
  `aleph3_notebook_core` only if a later approved task needs notebook document
  behavior, and `aleph_client` for protocol/framing.
- `aleph_client` must remain independent from kernel, parser, evaluator,
  session, SDK, pack, CLI, notebook, and web headers.
- Public protocol structs remain transport data. They must not grow fields that
  expose `Expr`, syntax trees, evaluator contexts, exact scalar storage, pack
  handler identities, or registry internals.

## Chosen Design

Use a small synchronous single-session server for M2. The process owns one
`session::Session` for its lifetime. Requests are handled serially in arrival
order. A later milestone may add process launch helpers, cancellation,
parallelism, notebook document orchestration, or richer lifecycle controls.

Framing remains the M1 shape:

```text
Content-Length: <byte-count>\r\n
\r\n
<json payload>
```

The executable should accept framed JSON on stdin and emit framed JSON on
stdout. Human-readable logging, if any, goes to stderr and must not corrupt the
framed stdout stream.

### Protocol Method Mapping

M2 implements these protocol methods:

| Method | Session/kernel behavior |
| --- | --- |
| `initialize` | Validate protocol version, return server name, version, protocol version, and capabilities. |
| `version` | Return executable and protocol version metadata. |
| `capabilities` | Return supported method names, representation keys, and bounded feature flags needed by clients. |
| `evaluate` | Execute source through `SessionOperation::evaluate`; preserve plain-text exact output. |
| `simplify` | Execute source through `SessionOperation::simplify`. |
| `fullForm` | Execute source through `SessionOperation::full_form` and return full-form text. |
| `help` | Execute source/query through `SessionOperation::help`; return structured help entries. |
| `complete` | Execute prefix through `SessionOperation::complete`; return structured completion entries. |
| `packages` | Execute `SessionOperation::discover_packs`; return deterministic package and symbol lists. |
| `reset` | Call `Session::reset`; return success metadata and preserve provider-owned symbols. |
| `shutdown` | Return a final success response and exit cleanly after the response is flushed. |

`inspect` remains private CLI behavior unless it is explicitly added to the
public protocol in a later approved slice. SDK validation, SDK compilation,
demo host functions, parser-token dumps, and parse-tree dumps remain private
developer CLI features and are not M2 protocol methods.

### Response Shape

Reuse and extend the M1 protocol data where needed:

- successful expression operations return `ok: true`, the original `source`,
  `representations["text/plain"]`, and optionally
  `representations["text/x-aleph-source"]` when the current renderer provides
  the same canonical text;
- session failures return protocol error envelopes with stable diagnostic
  codes/messages copied from `SessionResult::diagnostics`;
- help, completion, package, initialize, version, capabilities, reset, and
  shutdown return plain JSON objects or arrays, not private C++ identifiers;
- malformed JSON, unknown methods, invalid parameter types, incompatible
  protocol versions, invalid frames, oversized frames, and missing required
  fields fail deterministically.

## Non-Goals

- CLI migration through a spawned runtime process;
- public process-launching client library;
- fake-kernel launch tests for public clients;
- public-client-only build mode;
- physical repository movement;
- notebook UI integration;
- cancellation, concurrent requests, stream interruption, or background jobs;
- host-function registration over the protocol;
- exposing parser tokens, parse trees, SDK IR, SDK schemas, pack internals, or
  evaluator internals;
- changing symbolic semantics, exactness behavior, diagnostics, help content,
  or completion ownership.

## Implementation Tasks

### 1. Server Adapter Contract

Create a small private adapter that translates decoded protocol requests into
`session::Session` calls and protocol responses.

Completion criteria:

- request dispatch is centralized and covered by tests;
- unsupported methods and malformed params produce stable protocol errors;
- session diagnostics flow into protocol diagnostics without code rewriting
  beyond the existing session parse-error normalization;
- no symbolic behavior is implemented outside `session::Session` and existing
  kernel/pack registrations.

Focused tests:

- dispatch `evaluate` for `1/2 + 1/3` and preserve `5/6`;
- dispatch `simplify` for a simple simplification;
- dispatch `fullForm` for an expression without evaluating it;
- reject an unknown method and a params object with the wrong shape.

### 2. Metadata And Lifecycle Methods

Implement `initialize`, `version`, `capabilities`, `reset`, and `shutdown`.

Completion criteria:

- `initialize` rejects incompatible protocol versions clearly;
- `capabilities` lists only methods actually implemented in M2;
- `reset` clears session-local definitions while preserving builtins and packs;
- `shutdown` flushes its response before the process exits with code 0.

Focused tests:

- initialize/version/capabilities round trips;
- evaluate `a = 7`, evaluate `a`, reset, then evaluate `a` and observe the
  same session behavior as direct session tests;
- shutdown returns success and terminates cleanly.

### 3. Discovery Methods

Map `help`, `complete`, and `packages` to the existing session discovery
surface.

Completion criteria:

- help entries include name, category, owning package, forms, examples,
  exactness, unsupported boundaries, and manual anchor when available;
- completion entries include name, category, owning package, and documentation;
- package discovery is deterministic and agrees with session tests;
- session-local definitions appear in completion/help after evaluation and
  disappear after reset.

Focused tests:

- `complete` with `Fa` returns `Factor` from `core-algebra`;
- `help` with `Factor` returns the manual-backed algebra help entry;
- `packages` includes registered core packs and symbols in stable order;
- session-local function help/completion behavior matches `SessionTests`.

### 4. Executable IO Loop

Add the `aleph-runtime` executable that reads framed messages from stdin and
writes framed responses to stdout.

Completion criteria:

- adjacent requests are handled in order;
- malformed frames and malformed JSON produce deterministic protocol failures
  without crashing;
- stdout remains framed protocol output only;
- stderr is optional and suitable for private diagnostics;
- process exit codes distinguish clean shutdown from fatal startup/IO errors.

Focused tests:

- process-level test sends two adjacent requests and reads two framed
  responses;
- malformed frame handling is deterministic;
- shutdown exits cleanly after the framed response.

### 5. Build Integration

Add build targets without changing existing CLI behavior.

Targets:

```text
aleph-runtime
aleph_runtime_protocol_tests
```

Completion criteria:

- `aleph-runtime` builds only when the symbolic engine/kernel is enabled;
- `aleph_client` remains buildable without private semantic targets;
- `aleph_runtime_protocol_tests` run under CTest;
- existing kernel, SDK, CLI, notebook, and web tests remain wired as before.

Focused verification:

```text
cmake --build build --config Release --target aleph-runtime aleph_runtime_protocol_tests
ctest --test-dir build -C Release -R "aleph_client_tests|aleph_runtime_protocol_tests" --output-on-failure
```

Adjust the configuration name or build directory to match the local generator.

### 6. Documentation

Update documents that own the new current behavior once implementation lands:

- `docs/public_private_cli_split_plan.md` marks M2 implemented and makes M3
  the active detailed slice;
- `docs/architecture.md` records the private `aleph-runtime` process and
  dependency direction;
- `docs/sdk/build_and_targets.md` lists the executable and protocol process
  tests;
- `docs/manual/sessions-cli-and-notebook.md` explains that the protocol
  process exists for notebook/app clients while the CLI remains private
  first-party tooling;
- `docs/README.md` links to the active M3 plan and the archived M2 record when
  M2 is complete.

Do not present CLI migration, public process-launching clients, public notebook
use, or repository movement as shipped until later milestones implement them.

## Baseline And Verification

Before implementation:

```text
cmake --build build --config Release --target aleph_client_tests aleph3_symbolic_tests
ctest --test-dir build -C Release -R "aleph_client_tests|aleph3_symbolic_tests" --output-on-failure
```

During implementation, run focused protocol/server tests after each vertical
task. After implementation:

```text
cmake --build build --config Release --target aleph-runtime aleph_runtime_protocol_tests aleph_client_tests
ctest --test-dir build -C Release -R "aleph_client_tests|aleph_runtime_protocol_tests" --output-on-failure
ctest --test-dir build -C Release -R "aleph3_symbolic_tests|aleph3_sdk_tests|aleph3_notebook_tests" --output-on-failure
git diff --check
```

If the local Windows build hits MSBuild `FileTracker` or environment failures,
follow [Windows Codex Build Environment](../agents/windows-codex-build.md).

## Completion Criteria

M2 is complete when:

- `aleph-runtime` exists as a private executable and serves the M1 framed JSON
  protocol;
- all M2 protocol methods above are implemented through shared
  `session::Session` behavior;
- process-level protocol tests cover success, failure, reset, discovery,
  adjacent frames, malformed input, and shutdown;
- `aleph_client` still has no forbidden private semantic dependencies;
- the existing CLI remains behaviorally unchanged and private;
- documentation names the runtime process as implemented without claiming CLI
  migration, public client launch behavior, notebook integration, or repository
  split;
- focused and affected broader tests pass or any unrelated failures are
  recorded;
- the final diff passes whitespace checks and has been reviewed for duplicate
  semantics and stale public/private claims.

## Next Slice

After M2, proceed to M3: a process-launching runtime client library that can
locate `aleph-runtime`, send framed requests, handle timeouts and process exit,
surface missing/incompatible runtime diagnostics, and prove the public client
path against a fake kernel.
