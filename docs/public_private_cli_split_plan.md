# Public App / Private Kernel Split

## Status

This is the durable repository and process-boundary direction for Aleph. It is
not shipped behavior until the protocol boundary, packaging, and repository
split are implemented and verified.

This document complements [IP and Repository Strategy](ip_and_repo_strategy.md)
and the active sequencing in [Aleph3 Unified Plan](aleph3_unified_plan.md).
It should stay concise: detailed implementation tasks belong in issues, pull
requests, or the unified plan when they change roadmap priority.

M1, the protocol model, is implemented and archived in
[Aleph Runtime Protocol M1 Plan](archive/aleph_runtime_protocol_m1_plan.md).
M2, the private in-tree `aleph-runtime` executable, is implemented and archived
in [Aleph Runtime Protocol M2 Plan](archive/aleph_runtime_protocol_m2_plan.md).
M3, the process-launching runtime client, is implemented and archived in
[Aleph Runtime Protocol M3 Plan](archive/aleph_runtime_protocol_m3_plan.md).
M4, the private CLI migration through the runtime client, is implemented and
archived in [Aleph Runtime Protocol M4 Plan](archive/aleph_runtime_protocol_m4_plan.md).
The active detailed slice plan is
[Aleph Runtime Protocol M5 Plan](aleph_runtime_protocol_m5_plan.md).

## Direction

The current repository should become the private core repository, provisionally
`aleph-core`. It owns the semantic engine and first-party developer tooling:

- kernel expression representation, parser/lowering, evaluator, exact
  arithmetic, assumptions, rewriting, diagnostics, budgets, and registration;
- registered math packs and pack implementation code;
- session semantics and compatibility tests;
- the private CLI, currently `aleph3_cli`;
- the real `aleph-runtime` executable.

`aleph-runtime` is the private computation process, not the
domain-independent kernel core. It hosts the core kernel library, session
state, and registered packs behind the protocol. The kernel core remains
domain-independent; domain behavior belongs in registered packs.

Public visibility should move to a product repository, provisionally
`aleph-notebook`. It owns product code that can be built and tested without
private sources:

- notebook UI and local document workflows;
- public protocol/client code, unless that later deserves its own
  `aleph-client` repository;
- protocol documentation, examples, compatibility notes, and runtime setup;
- fake-kernel tests for notebook/client behavior.

The first public/private boundary is process-based:

```text
public notebook / app clients        private CLI
              |                         |
              | framed JSON protocol    | framed JSON protocol
              v                         v
              private aleph-runtime executable
                       |
                       v
              private aleph-core implementation
```

The CLI remains private first-party tooling unless a separate public CLI is
explicitly designed later. The public product surface is the notebook/client
layer and the stable runtime protocol, not public C++ kernel headers.

Before building the public notebook repository, the process boundary should be
rehearsed inside the private repository. The private CLI is the first real
consumer of the launchable runtime client. A later private notebook-lite should
exercise notebook-shaped workflows over the same boundary so the public
notebook starts from a stable protocol, client lifecycle, diagnostics, and
document interaction model.

## Boundary Rules

Public repositories may contain protocol-facing data types, process-launching
client code, notebook documents, UI code, examples, and fake-kernel tests.

Public repositories must not expose or depend on private implementation types
or headers such as:

```text
Expr
ExprPtr
FunctionCall
EvaluationContext
FunctionRegistry internals
parser node internals
pack handler types
exact arithmetic internals
```

If a public client needs information that appears to require those internals,
that is a protocol-design gap. Fix the protocol instead of coupling the client
to private code.

The notebook, future app clients, and future agent integrations should use the
same runtime protocol for ordinary semantic operations. Agent-facing behavior
belongs inside this runtime boundary or an explicitly versioned extension of
it. Do not introduce a separate agent semantic protocol until the runtime
protocol has proven insufficient.

## Protocol Shape

Use an explicit framed JSON protocol. The initial framing is:

```text
Content-Length: <byte-count>\r\n
\r\n
<json payload>
```

Use JSON-RPC-like request and response envelopes. The first public methods are:

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

Protocol result types are plain transport data: text representations,
diagnostics, capability metadata, help entries, package lists, and request
status. They must not expose `Expr`, syntax trees, evaluator state, exact
scalar storage, or pack registration internals.

Version negotiation starts in `initialize`. Clients should fail clearly when
the runtime is missing or the protocol version is incompatible.

Runtime lookup order:

1. explicit `--runtime <path>` argument where the client has one;
2. `ALEPH_RUNTIME_PATH`;
3. same directory as the client executable;
4. `PATH` lookup.

## Migration Milestones

Keep the physical repository split until the process boundary is proven inside
the current tree.

1. **Protocol model.** Initial implementation complete. Add extraction-ready
   protocol types, framing, JSON encode/decode, malformed-message diagnostics,
   and documentation. This layer must not include private kernel, parser,
   session, SDK, pack, or CLI headers.
2. **Private `aleph-runtime`.** Complete. The executable hosts the
   domain-independent kernel core, one session, and registered packs, then
   serves protocol requests for evaluation, simplification, full form, help,
   completion, package discovery, reset, initialization, and shutdown.
3. **Kernel client.** Complete. The client library locates and launches
   `aleph-runtime`, sends framed requests, handles timeouts and process exit,
   and reports missing or incompatible runtimes clearly. It is tested with a
   fake runtime and a real private-runtime smoke path.
4. **Private CLI migration.** Complete. Ordinary symbolic CLI behavior routes
   through the kernel client. Parser-token dumps, SDK validation/compile
   tooling, demo host-function commands, and private inspection remain private
   developer tools unless separately productized or promoted through a separate
   protocol design.
5. **Private notebook-lite.** Add an internal notebook-shaped harness or small
   app over the runtime client. It exercises document create/edit/evaluate,
   save/reopen, `Run All`, reset, help, completion, package discovery,
   diagnostics, stale output handling, and runtime failure display without
   starting the public notebook repository or adding UI-owned semantics.
6. **Public-client fixture.** Prove a build mode or fixture that includes only
   extraction-ready protocol/client/notebook-facing code and fake-kernel tests.
   It must not compile the CLI, private kernel executable, private packs, or
   semantic implementation tests.
7. **Physical split.** Make the current repository private as `aleph-core` only
   after the private CLI and fake public client both exercise the protocol
   boundary. Create the public `aleph-notebook` repository with notebook,
   protocol/client, examples, docs, and fake-kernel tests.
8. **Packaging and release checks.** Private CI builds signed or provenance-
   tracked `aleph-runtime` and private CLI artifacts. Public CI builds the
   notebook/client against fake kernels. Release checks must prevent private
   source, headers, debug symbols, private paths, and private CI logs from
   leaking into public artifacts.

These milestones are enough for this document. If an implementation slice
needs command-by-command migration detail, record it in the working issue or
task plan and keep this file as the stable boundary contract.

## Testing

Public verification:

- protocol encode/decode and framed IO tests;
- fake-kernel tests for launch, request orchestration, diagnostics, help,
  completion, package discovery, reset, and missing-kernel behavior;
- public app-client build with private sources disabled.

Private verification:

- kernel, session, pack, and private CLI tests;
- real `aleph-runtime` protocol tests;
- private CLI to real kernel integration tests;
- private notebook-lite to real `aleph-runtime` integration tests;
- product-bundle smoke tests where the public notebook/client finds and uses
  the packaged private kernel executable.

## Completion Criteria

The split is complete when:

- public app-client code includes no private kernel, parser, expression,
  evaluator, exact arithmetic, CLI, or pack implementation headers;
- public app-client builds and tests pass without private source files;
- private `aleph-runtime` passes kernel/session/pack tests;
- private CLI plus private `aleph-runtime` pass integration tests;
- private notebook-lite proves notebook workflows through the runtime client;
- public notebook/client plus private `aleph-runtime` pass bundle smoke tests;
- protocol versioning and incompatible-version diagnostics exist;
- documentation accurately distinguishes public source from private semantics;
- release checks prevent accidental publication of private implementation
  artifacts.
