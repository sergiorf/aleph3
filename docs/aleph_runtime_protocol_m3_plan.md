# Aleph Runtime Protocol M3 Plan

## Status

This is the active detailed implementation plan for M3 from
[Public App / Private Kernel Split](public_private_cli_split_plan.md): a
process-launching runtime client library over the implemented private
`aleph-runtime` protocol server.

M1, the protocol model, and M2, the private runtime executable, are complete
and archived in [docs/archive](archive/README.md). M3 should be completed
inside the current tree before CLI migration, private notebook-lite, public
notebook integration, or physical repository movement.

Public notebook work should not start from this plan. The new process boundary
should be proven first with the private CLI as the next real consumer, after
M3 delivers the launchable client library. A private notebook-lite rehearsal
may follow CLI migration as a later internal consumer.

## Goal

Add a public-client-facing process layer that can locate and launch
`aleph-runtime`, exchange framed protocol requests and responses, handle
timeouts and process exit, and report missing or incompatible runtimes with
stable diagnostics.

The slice succeeds when client tests can drive a fake runtime and the real
runtime through the same framed lifecycle without linking private kernel,
parser, evaluator, session, pack, CLI, notebook, or SDK implementation
headers.

## Roadmap Alignment

- Advances milestone 3, **Kernel client**, in the public/private split plan.
- Supports the notebook-first product path by giving future public notebook
  and app-client code one launch/request lifecycle over the runtime protocol.
- Keeps the private `aleph-runtime` process as the host for kernel, session,
  and registered packs.
- Establishes the boundary that M4 will test through ordinary private CLI
  behavior before private notebook-lite or public graphical notebook work
  depends on it.
- Does not change mathematical behavior, the supported symbolic subset, CLI
  behavior, notebook document behavior, or repository visibility.

## Ownership

The M3 client process layer belongs with the public protocol/client component:

```text
include/aleph_client/
src/aleph_client/
tests/aleph_client/
```

Exact file names may vary, but the likely shape is:

```text
include/aleph_client/RuntimeClient.hpp
src/aleph_client/RuntimeClient.cpp
tests/aleph_client/RuntimeClientTests.cpp
tests/aleph_client/fakes/
```

The existing `aleph_client` protocol and framing helpers remain the lower
layer. The new runtime client may depend on them and on platform process/pipe
APIs. It must remain independent from private semantic headers and targets.

Allowed dependencies:

- C++ standard library;
- platform process primitives used behind the public client abstraction;
- existing `aleph_client` protocol and framing helpers;
- the repository-standard test framework in tests.

Forbidden dependencies:

- `expr`, `kernel`, `evaluator`, `symbols`, `normalizer`, `transforms`;
- `syntax`, parser internals, frontend parse trees, or SDK IR;
- `session`;
- `sdk`;
- `packs`, algebra, calculus, or future domain implementation headers;
- `tooling`, `RuntimeProtocolServer`, `aleph3_cli`, or private CLI
  presentation code;
- notebook, web, GUI, or BFF code.

Real-runtime smoke tests may depend on the `aleph-runtime` executable target as
an external process artifact. They must not include private runtime server or
semantic headers.

If a useful client result appears to require a forbidden dependency, treat that
as a protocol-design gap. Add or revise transport data deliberately instead of
coupling public client code to private implementation types.

## Chosen Design

Add a synchronous single-request-at-a-time runtime client for M3. The client
owns a child process, writes one framed request to the child stdin, reads one
framed response from child stdout, decodes the response with existing
`aleph_client` protocol helpers, and exposes stable client-side failures when
the process cannot be used.

The M3 API should be small and boring. Prefer value types and explicit
lifecycle methods over callbacks, background workers, or UI-specific behavior.

Representative public API concepts:

- `RuntimeClientOptions`: explicit runtime path, client name/version,
  protocol version, startup timeout, request timeout, shutdown timeout,
  maximum frame size, and optional environment overrides for tests.
- `RuntimeLookupResult`: selected executable path and source, such as explicit
  option, `ALEPH_RUNTIME_PATH`, same-directory lookup, or `PATH`.
- `RuntimeClient`: start, initialize, request, shutdown, and termination-safe
  cleanup.
- `RuntimeClientError`: stable code, message, and optional protocol
  diagnostics for failures before or outside a decoded protocol response.

Concrete names may change to fit local style. The contract is that public
clients can launch a runtime, send protocol methods, receive decoded
`ProtocolResponse` values, and distinguish client lifecycle failures from
runtime protocol errors.

### Runtime Lookup

Runtime lookup follows the split-plan order:

1. explicit path in client options, or a future command argument supplied by a
   consumer;
2. `ALEPH_RUNTIME_PATH`;
3. same directory as the client executable;
4. `PATH` lookup.

Lookup must be deterministic and testable. Tests should be able to provide a
fake runtime path without mutating global machine state. Missing, non-file,
non-executable, and launch-denied candidates should produce stable client
errors with enough path/source context for a caller to present a useful
message.

Do not add package discovery, install repair, downloads, registry probing, or
product-specific runtime setup in M3.

### Lifecycle

The M3 lifecycle is:

1. resolve the runtime executable path;
2. start the process with stdin/stdout pipes;
3. send `initialize` with client name, client version, and protocol version;
4. reject incompatible protocol versions clearly;
5. send one request at a time using existing framed JSON helpers;
6. decode success or error responses using existing protocol helpers;
7. send `shutdown` during normal close and wait for process exit;
8. terminate or detach only as a last-resort cleanup path after timeout or
   broken pipe.

One `RuntimeClient` instance talks to one launched `aleph-runtime` process, and
that process owns one `session::Session` for its lifetime. Session-local
assignments and user definitions therefore persist across requests sent
through the same client, for example `a = 7` followed by `a` returns `7`.
`reset` clears that session-local state while preserving builtins and
registered packs. A new client process starts with a fresh runtime session.

The client must preserve stdout as framed protocol data and treat stderr as
diagnostic text only. Stderr may be captured behind a bounded buffer for error
messages, but no public behavior may depend on private log wording.

M3 is synchronous and serial. Cancellation, streaming output, concurrent
requests, process pools, automatic restart, long-lived notebook recovery, and
interactive background jobs are later milestones.

### Boundary Proof Order

Use the private CLI as the first real consumer of the runtime-client boundary.
Public notebook work should remain on hold until the CLI path has exercised
ordinary symbolic evaluation, reset, help, completion, package discovery,
protocol failure reporting, and runtime lookup through the new client. After
that, a private notebook-lite can rehearse document and notebook interaction
over the same boundary before the public notebook begins.

### Request Surface

The process client should expose generic request/response plumbing plus small
convenience wrappers only where they are already protocol-level operations:

- `initialize`;
- `version`;
- `capabilities`;
- `evaluate`;
- `simplify`;
- `fullForm`;
- `help`;
- `complete`;
- `packages`;
- `reset`;
- `shutdown`.

Wrappers must only encode protocol messages and decode transport responses.
They must not parse expressions, interpret outputs, synthesize mathematical
diagnostics, retry semantic operations, or add UI/notebook policy.

### Diagnostics

M3 introduces client lifecycle diagnostics, separate from protocol errors
returned by `aleph-runtime`. Use stable error codes for at least:

| Code | Situation |
| --- | --- |
| `runtime.not_found` | no runtime candidate can be resolved |
| `runtime.not_executable` | a selected candidate cannot be launched |
| `runtime.launch_failed` | process creation fails for another reason |
| `runtime.initialize_failed` | startup succeeds but initialize has no usable success response |
| `runtime.incompatible_version` | initialize reports or reveals an unsupported protocol version |
| `runtime.timeout` | startup, request, or shutdown exceeds the configured timeout |
| `runtime.exited` | process exits before a complete response is read |
| `runtime.broken_pipe` | writing a request fails because the process is unavailable |
| `runtime.malformed_output` | stdout cannot be decoded as a valid frame or protocol response |
| `runtime.shutdown_failed` | graceful shutdown does not complete |

If the runtime returns a protocol error envelope, preserve that protocol code
and diagnostics as runtime data rather than remapping it to a lifecycle error.
Only failures in lookup, process management, framing, timeout, and response
decoding use the `runtime.*` client codes above.

## Non-Goals

- migrating `aleph3_cli` through the runtime client;
- public notebook integration or public notebook toolkit work;
- private notebook-lite implementation;
- physical repository movement;
- public-client-only build mode beyond focused fake-runtime client tests;
- packaging, signing, install repair, or runtime download behavior;
- cancellation, streaming output, concurrent requests, process pools, or
  automatic restart;
- host-function registration over the protocol;
- adding new protocol methods;
- changing `aleph-runtime` server semantics except for defects discovered by
  client tests;
- exposing parser tokens, parse trees, SDK IR, exact scalar storage, pack
  internals, evaluator internals, or private debug commands.

## Implementation Tasks

### 1. Repository Research And Baseline

Confirm the current target layout, public include boundary, runtime executable
location in the build tree, and platform process support already available in
the repository or standard library.

Completion criteria:

- record the chosen process primitive for Windows and non-Windows builds;
- identify how tests will find fake and real runtime executables;
- preserve the existing `aleph_client` no-private-include guard;
- run or record the nearest available baseline for protocol and runtime tests.

Focused verification:

```text
cmake --build build --config Release --target aleph_client_tests aleph-runtime aleph_runtime_protocol_tests
ctest --test-dir build -C Release -R "aleph_client_tests|aleph_runtime_protocol_tests" --output-on-failure
```

Adjust the configuration name or build directory to match the local generator.

### 2. Fake Runtime Test Harness

Add a tiny fake runtime executable or script used only by
`aleph_client` tests. It should read and write the same framed JSON protocol as
`aleph-runtime` without linking private semantic targets.

Completion criteria:

- fake runtime can return a successful initialize response;
- fake runtime can echo or synthesize success responses for representative
  requests;
- fake runtime can simulate incompatible protocol version, malformed stdout,
  early exit, delayed response, and shutdown;
- test fixtures do not require a private kernel, session, pack, CLI, or
  runtime server include.

Focused tests:

- launch fake runtime through an explicit path;
- initialize succeeds and reports protocol version 1;
- shutdown returns success and exits cleanly;
- fake delayed response triggers the configured timeout.

### 3. Runtime Lookup

Implement deterministic runtime path resolution behind the public client API.

Completion criteria:

- explicit path wins over all other sources;
- `ALEPH_RUNTIME_PATH` is honored when no explicit path is supplied;
- same-directory lookup and `PATH` lookup are represented in the API and
  covered where practical;
- missing and unlaunchable candidates produce stable `runtime.*` diagnostics;
- tests avoid persistent mutation of the developer machine environment.

Focused tests:

- explicit fake runtime path is selected;
- missing explicit path reports `runtime.not_found` or
  `runtime.not_executable` as appropriate;
- environment override selects the fake runtime;
- lookup result records the selected source.

### 4. Process Lifecycle Client

Implement the synchronous process wrapper that starts the runtime, writes
framed requests, reads framed responses, and performs cleanup.

Completion criteria:

- process pipes are binary-safe on Windows;
- stdout parsing accepts adjacent frames and rejects malformed output;
- stderr capture, if implemented, is bounded and never parsed as protocol;
- destructors or close methods clean up child processes without hanging;
- lifecycle failures report stable `runtime.*` diagnostics.

Focused tests:

- start fake runtime, send initialize, send evaluate, send shutdown;
- send `a = 7` and then `a` through the same client and observe the persisted
  session value;
- send `reset` through the same client and observe that `a` becomes symbolic
  again while builtins and packs remain available;
- process exit before response reports `runtime.exited`;
- malformed frame or malformed JSON reports `runtime.malformed_output`;
- request write after process death reports `runtime.broken_pipe` or
  `runtime.exited`;
- timeout leaves no long-lived fake child process.

### 5. Protocol Convenience Operations

Add convenience request methods only for existing M1/M2 protocol operations.

Completion criteria:

- convenience methods reuse existing protocol encoders/decoders or add missing
  public protocol encoders where needed;
- request identifiers remain deterministic enough for tests;
- protocol error envelopes remain visible as protocol errors, not lifecycle
  failures;
- no convenience method interprets symbolic output beyond returning decoded
  transport data.

Focused tests:

- fake runtime receives and responds to `evaluate`, `simplify`, `fullForm`,
  `help`, `complete`, `packages`, `reset`, `version`, and `capabilities`;
- a fake protocol error envelope preserves its original code and diagnostics;
- malformed params are not synthesized by the client for semantic validation.

### 6. Real Runtime Smoke Coverage

Add focused smoke tests that launch the built private `aleph-runtime` through
the same public client path when private runtime targets are enabled.

Completion criteria:

- test target depends on the `aleph-runtime` executable artifact, not private
  runtime headers;
- initialize succeeds against the real runtime;
- evaluating `1/2 + 1/3` returns `5/6`;
- evaluating `a = 7` and then `a` proves session state persists within one
  client/runtime process;
- reset clears the session-local definition through the process client;
- shutdown exits cleanly.

Focused verification:

```text
cmake --build build --config Release --target aleph-runtime aleph_client_tests aleph_runtime_protocol_tests
ctest --test-dir build -C Release -R "aleph_client_tests|aleph_runtime_protocol_tests" --output-on-failure
```

### 7. Boundary And Build Integration

Wire the new source and tests into CMake without changing existing CLI or
runtime server behavior.

Completion criteria:

- `aleph_client` remains buildable without `aleph3_kernel`, `aleph3_sdk`,
  `aleph_runtime_protocol`, private packs, or CLI targets;
- no private semantic includes appear under `include/aleph_client`,
  `src/aleph_client`, or `tests/aleph_client` fake-runtime client tests;
- real-runtime smoke coverage is gated behind the existing private runtime
  build availability;
- existing runtime server and CLI targets do not start using the client in M3.

Focused tests:

- extend the existing `aleph_client` boundary guard to cover new files;
- verify CMake target dependencies by review and, where available, generated
  target graph or build failure when private targets are disabled.

### 8. Documentation

Update documents that own the new current behavior once implementation lands:

- `docs/public_private_cli_split_plan.md` marks M3 implemented and makes M4,
  private CLI migration, the next split milestone;
- `docs/architecture.md` records the public process-launching client layer and
  dependency direction;
- `docs/sdk/build_and_targets.md` lists the new client source/test coverage
  and explains which tests use fake versus real runtimes;
- `docs/manual/sessions-cli-and-notebook.md` explains runtime lookup,
  lifecycle failures, and the fact that CLI migration and notebook integration
  are still planned;
- `docs/README.md` links to the archived M3 record if the document is moved
  after completion.

Do not present public notebook use, CLI migration, packaging, or repository
movement as shipped until later milestones implement them.

## Baseline And Verification

Before implementation:

```text
cmake --build build --config Release --target aleph_client_tests aleph-runtime aleph_runtime_protocol_tests
ctest --test-dir build -C Release -R "aleph_client_tests|aleph_runtime_protocol_tests" --output-on-failure
```

During implementation, run focused fake-runtime tests after each vertical task.
After implementation:

```text
cmake --build build --config Release --target aleph_client_tests aleph-runtime aleph_runtime_protocol_tests
ctest --test-dir build -C Release -R "aleph_client_tests|aleph_runtime_protocol_tests" --output-on-failure
ctest --test-dir build -C Release -R "aleph3_symbolic_tests|aleph3_sdk_tests|aleph3_notebook_tests" --output-on-failure
git diff --check
```

If the local Windows build hits MSBuild `FileTracker` or environment failures,
follow [Windows Codex Build Environment](agents/windows-codex-build.md).

Review the final diff for forbidden dependencies, duplicate process lifecycle
logic, stale public/private claims, leaked private paths in public docs, and
any accidental user-visible semantic behavior change.

## Completion Criteria

M3 is complete when:

- the public runtime client can locate and launch a runtime using the split
  plan lookup order;
- fake-runtime lifecycle tests cover launch, initialize, request/response,
  session state, reset, timeout, early exit, malformed output, protocol errors,
  and shutdown;
- real-runtime smoke tests pass where private targets are enabled;
- runtime lookup, timeout, process-exit, malformed-output, missing-runtime,
  and incompatible-version failures have stable diagnostics;
- `aleph_client` still has no forbidden private semantic dependencies;
- existing CLI behavior is unchanged and not yet routed through the client;
- documentation explains runtime lookup and failure behavior without claiming
  CLI migration, public notebook integration, packaging, or repository split;
- the plan and docs identify private CLI migration as the next boundary proof
  before private notebook-lite or public notebook implementation starts;
- focused and affected broader tests pass or any unrelated failures are
  recorded;
- `git diff --check` passes and the final diff has been reviewed.

## Next Slice

After M3, proceed to
[M4](aleph_runtime_protocol_m4_plan.md): migrate ordinary private CLI symbolic
behavior through the kernel client and use that path to prove the
public/private process boundary. Keep parser-token dumps, SDK validation/
compile tooling, and demo host-function commands private developer tools
unless a separate product design promotes them. After M4, proceed to
[M5](aleph_runtime_protocol_m5_plan.md), a private notebook-lite rehearsal over
the same runtime boundary. Start public notebook integration only after those
boundary proofs are implemented, tested, and documented.
