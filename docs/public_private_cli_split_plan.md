# Public App / Private Kernel Split Plan

## Status

This is a proposed implementation plan. It records the intended first stage of
a public/private repository split for Aleph3, scoped to a private kernel
process and public product clients such as the notebook and future agent
integrations. The CLI is retained as a private first-party client and developer
tool. This plan does not describe shipped behavior until the milestones are
implemented and the owning specifications are updated.

This plan complements the practical guidance in
[IP and Repository Strategy](ip_and_repo_strategy.md). It is engineering
guidance, not legal advice.

## Goal

Refactor Aleph3 so public product clients can be built, tested, and distributed
without containing private kernel, parser, evaluator, exact arithmetic, CLI, or
math pack implementations.

The private CLI and public product clients communicate with a separately built
private `aleph-kernel` executable through the same versioned local JSON
protocol. The CLI remains in the private Aleph repository so kernel, protocol,
diagnostic, and developer-tool iteration stays fast.

The initial product boundary is:

```text
public notebook / app clients        private CLI
              |                         |
              | framed JSON protocol    | framed JSON protocol
              v                         v
          private aleph-kernel executable
                       |
                       v
          private libaleph + private packs
```

## Non-Goals

- notebook UI implementation;
- Agent or LLM integration;
- Ollama integration;
- Qt toolkit selection;
- dynamic pack marketplace or runtime pack unloading;
- public CLI extraction;
- public C++ kernel headers;
- semantic changes to Aleph expressions, evaluation, exact arithmetic, or
  supported mathematics.

## Public And Private Ownership

The public repository should contain:

- public kernel protocol and client types;
- process-launching client code;
- notebook document format and later notebook UI code when that work resumes;
- generic agent framework and provider adapters when that work becomes active;
- protocol, product-client, and fake-kernel tests;
- public documentation, examples, and compatibility notes.

The private repository should contain:

- `libaleph`;
- the CLI application source, presentation code, and developer/debug commands;
- parser and symbolic lowering;
- `Expr` and the expression model;
- evaluator and runtime;
- exact integer, rational, real, and complex implementations;
- symbol/session state;
- function and package registration internals;
- simplification, rewriting, assumptions, and diagnostics implementation;
- Algebra, Calculus, Linear Algebra, and future domain pack implementations;
- the real `aleph-kernel` executable;
- real CLI-to-kernel integration tests;
- advanced Aleph-specific Agent reasoning if it becomes a product
  differentiator.

Agent ownership follows the same boundary as notebook ownership. Public app
repositories may own generic agent UI, provider adapters, message/session
schemas, tool-call envelopes, permission prompts, and workflows that consume
only the stable `aleph-kernel` protocol. The private repository owns any
Aleph-specific symbolic planning, tutoring strategy, proof or equivalence
heuristics, pack-aware reasoning, prompt/tool policies that reveal private
implementation structure, or tools that depend on `Expr`, evaluator, registry,
assumption, or pack internals.

Do not introduce a separate agent semantic protocol before the kernel protocol
has proven insufficient. Initial agent workflows should use the same stable
methods as the notebook and private CLI, such as `evaluate`, `help`,
`complete`, `packages`, `reset`, `inspect`, and later any explicitly specified
semantic method such as transformation validation.

The public repository must not expose or depend on:

```text
Expr
ExprPtr
FunctionCall
EvaluationContext
FunctionRegistry internals
pack handler types
parser node internals
exact arithmetic internals
```

## Target Repository Shape

The public repository should evolve toward a notebook and app-client project
such as `aleph-notebook`:

```text
apps/
  notebook/
  agent/

include/
  aleph_client/
    KernelClient.hpp
    KernelProtocol.hpp
    KernelProcess.hpp

src/
  aleph_client/
    KernelClient.cpp
    KernelProtocol.cpp
    KernelProcess.cpp

tests/
  notebook/
  agent/
  protocol/
  fake_kernel/

docs/
  kernel_protocol.md
  runtime_setup.md
  examples/
```

The current Aleph repository should become the private repository and evolve
toward:

```text
apps/
  cli/
  aleph-kernel/

include/ or libaleph/
  aleph_client/
  core/
  parser/
  expr/
  evaluator/
  runtime/
  session/
  formatting/

packs/
  algebra/
  calculus/
  linear_algebra/

tests/
  cli/
  protocol/
  kernel_process/
  session/
  packs/
```

The first physical split should happen only after the in-repo process boundary
is exercised by the private CLI and a fake public client. Until then, keep the
monorepo fast and use the protocol boundary as the architectural constraint.

## Protocol Boundary

Use a JSON protocol with explicit message framing. The preferred initial
framing is:

```text
Content-Length: <byte-count>\r\n
\r\n
<json payload>
```

The implementation must not assume arbitrary stdin reads correspond one-to-one
with complete JSON requests.

Use a JSON-RPC-like envelope:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "evaluate",
  "params": {
    "source": "1/2 + 1/3"
  }
}
```

Example response:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "ok": true,
    "source": "1/2 + 1/3",
    "representations": {
      "text/plain": "5/6",
      "text/x-aleph-source": "5/6"
    },
    "diagnostics": []
  }
}
```

Example error response:

```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "error": {
    "code": "kernel.division_by_zero",
    "message": "Division by zero is not allowed."
  }
}
```

Minimum first-client methods:

- `initialize`
- `evaluate`
- `simplify`
- `fullForm`
- `help`
- `complete`
- `packages`
- `reset`
- `shutdown`

Useful early additions:

- `version`
- `capabilities`

## Public Protocol Types

Public and private clients may define plain transport-facing types such as:

```cpp
struct KernelDiagnostic {
    std::string code;
    std::string severity;
    std::string message;
    std::optional<SourceSpan> span;
};

struct KernelEvaluationResult {
    bool ok;
    std::string source;
    std::map<std::string, std::string> representations;
    std::vector<KernelDiagnostic> diagnostics;
};

struct KernelHelpEntry {
    std::string name;
    std::string category;
    std::string owning_package;
    std::string description;
    std::vector<std::string> forms;
    std::vector<std::string> examples;
};
```

These types are protocol data. They must not expose private expression,
evaluator, parser, or exact arithmetic implementation details.

## Version Negotiation

Protocol versioning starts with `initialize`.

Request:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "initialize",
  "params": {
    "client": "aleph3-cli",
    "clientVersion": "0.1.0",
    "protocolVersion": 1
  }
}
```

Response:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "kernelVersion": "0.1.0",
    "protocolVersion": 1,
    "capabilities": {
      "evaluate": true,
      "help": true,
      "complete": true,
      "latex": false
    }
  }
}
```

Clients should fail clearly when the kernel protocol version is incompatible.

## Kernel Location

The private CLI and public product clients should find `aleph-kernel` in this
order:

1. explicit `--kernel <path>` argument;
2. `ALEPH_KERNEL_PATH`;
3. same directory as the CLI executable;
4. `PATH` lookup.

If no compatible kernel is found, the client should report a clear diagnostic
and avoid implying that public app code can perform private mathematical
execution by itself.

## Milestones

### M0 - Audit And Freeze The Boundary

Produce a short architecture note or plan section listing:

- current CLI commands;
- which commands require kernel behavior;
- which commands are SDK or trusted-frontend leftovers;
- which code paths currently include private headers;
- which outputs must remain stable;
- which tests prove current behavior.

Decide which current CLI behaviors should become protocol-backed first-party
client behavior and which should remain private developer/debug tooling. For
the repository split, symbolic CLI behavior should move through
`aleph-kernel`; broader SDK cleanup can be deferred unless it blocks the
process boundary.

#### M0 Boundary Audit

The CLI command name remains `aleph3_cli` inside the private repo. A later
product rename may introduce `aleph` or another shorter launcher after the
notebook and distribution shape are clearer; the repository split should not
combine a process-boundary architecture change with a command rename.

The private executable name is `aleph-kernel`.

The user-facing direction is a public notebook and app-client layer whose
normal evaluation, help, completion, package discovery, reset, and later agent
workflows route through the kernel process. The private CLI should use that
same protocol for its ordinary symbolic behavior, so it proves the process
boundary before the public notebook replaces it as the main product client.
The protocol should be intent-shaped and stable, with plain textual source and
result representations, structured diagnostics, and capability metadata. It
should not expose parser tokens, SDK IR nodes, `Expr`, evaluator internals,
exact arithmetic storage, or pack registration implementation details.
AI-facing and natural-language-friendly behavior belongs in help text,
examples, documentation, and later higher-level commands; the process protocol
itself stays deterministic and typed.

Current command inventory:

| Command or mode | Current implementation path | Split classification |
| --- | --- | --- |
| no arguments / `repl` | starts the interactive CLI; default mode is symbolic when `ALEPH3_HAS_SYMBOLIC_ENGINE` is enabled | private shell that should use `KernelClient` for ordinary symbolic work |
| bare CLI expression | falls through to `run_default_expression`, symbolic when available | private first-party evaluation surface; route through `aleph-kernel` |
| `help`, `--help`, `-h` | static CLI presentation text plus symbolic help in the REPL | private presentation; symbolic help entries route through `aleph-kernel` |
| `examples` | static CLI examples | private presentation; public examples live in the public app repo |
| `script [--json] <path>` | owns file reading, limits, JSON Lines rendering, and a `session::Session` for stateful evaluation | private first-party command; evaluation/reset state should route through `aleph-kernel` while file IO and JSON Lines formatting stay client-owned |
| `host-functions` | prints demo SDK host function docs from `tooling/DemoHostFunctions` | SDK-era demo command; private developer/demo tooling |
| `tokens <formula>` | calls `frontend::Lexer` and prints token internals | private developer/debug tooling; do not carry into the first public protocol |
| `parse <formula>` | calls `frontend::Parser` and prints SDK IR internals | private developer/debug tooling; do not carry into the first public protocol |
| `validate <formula>` | calls `sdk::Engine::validate` with an empty schema | SDK-era command; private developer/tooling unless separately productized |
| `compile <formula>` | calls `sdk::Engine::compile` and reports compile success | SDK-era command; private developer/tooling unless separately productized |
| `evaluate [--var ...] <formula>` | calls `sdk::Engine::compile` and `evaluate` with CLI bindings | SDK-era command; private developer/tooling unless separately productized |
| `evaluate-host [--var ...] <formula>` | registers demo host functions, then calls SDK compile/evaluate | SDK-era demo command; private developer/demo tooling |
| `symbolic-evaluate <expr>` | calls symbolic CLI helpers, which use private symbolic behavior | compatibility command; route through `aleph-kernel` |
| `symbolic-simplify <expr>` | calls symbolic CLI helpers, which use private symbolic behavior | compatibility command; route through `aleph-kernel` |
| `symbolic-fullform <expr>` | calls symbolic CLI helpers, which use private symbolic behavior | compatibility command; route through `aleph-kernel` |

Current REPL command inventory:

| REPL command | Current implementation path | Split classification |
| --- | --- | --- |
| bare input | uses active mode, symbolic by default when available | private first-party evaluation surface; route through `aleph-kernel` |
| `:help [name-or-prefix]` | static command help or `SessionOperation::help` | private CLI command; symbolic entries route through `aleph-kernel` |
| `:examples` | static examples | private presentation |
| `:mode [sdk|symbolic]` | switches between local SDK and symbolic execution modes | private transitional command; do not make dual evaluators a public product concept |
| `:host-functions` | static demo SDK host function docs | SDK-era demo command; private developer/demo tooling |
| `:tokens`, `:parse`, `:validate`, `:compile`, `:evaluate`, `:evaluate-host` | same local lexer/parser/SDK paths as top-level commands | private developer/debug tooling; do not include in the first public kernel protocol |
| `:symbolic-evaluate`, `:symbolic-simplify`, `:symbolic-fullform` | symbolic helper paths | compatibility commands; route through `aleph-kernel` |
| `:inspect <expr>` | `SessionOperation::inspect` | private diagnostic command; route through `aleph-kernel` if retained |
| `:packs` | `SessionOperation::discover_packs` | private discovery command; route through `aleph-kernel` |
| `:complete <prefix>` | `SessionOperation::complete` | private discovery command; route through `aleph-kernel` |
| `:reset` | `session::Session::reset` | private session lifecycle command; route through `aleph-kernel` |
| `:quit`, `:exit` | local REPL control | private shell behavior; stays in CLI |

Current private-header dependencies in `src/tooling/aleph3_cli.cpp` include:

- `frontend/Lexer.hpp` and `frontend/Parser.hpp` for `tokens` and `parse`;
- `ir/Node.hpp` for parser tree printing;
- `sdk/Engine.hpp` for SDK validation, compilation, evaluation, and demo host
  function evaluation;
- `session/Session.hpp` for REPL discovery, session evaluation, scripts,
  completion, help, packs, inspection, and reset;
- `tooling/SymbolicCliSupport.hpp` when `ALEPH3_HAS_SYMBOLIC_ENGINE` is
  enabled.

The first split should move symbolic execution, simplification, full form,
scripts, REPL bare evaluation, help, completion, pack discovery, inspection,
and reset behind `aleph-kernel` inside the private repo. It should not attempt
to expose token streams, parser trees, SDK IR, or demo host functions through
the public protocol. If those SDK-era developer tools remain useful, they can
remain private CLI commands or receive a separate public SDK-tooling decision
after the kernel-process boundary is stable.

Outputs to preserve during the compatibility phase:

- plain text result rendering for bare CLI expressions and `symbolic-*`
  commands;
- REPL prompt and command behavior where practical, especially bare input,
  `:help`, `:complete`, `:packs`, `:reset`, `:inspect`, and `:quit`;
- script continuation after failed lines, line-numbered diagnostics, exit code
  `2` when any line fails, and line-size failure exit code `3`;
- `script --json` JSON Lines fields `schema_version`, `line`, `source`, `ok`,
  `output`, and `diagnostics`, with exact outputs preserved as strings;
- deterministic missing-kernel and incompatible-protocol diagnostics in
  private CLI and public app clients once the process boundary exists.

Existing behavior evidence is concentrated in `tests/tooling/Aleph3CliTests.cpp`.
It covers bare expression evaluation, REPL meta commands and help, mode
switching, pack-backed matrix and calculus examples, session state, inspection,
completion, reset, cleanup precedence, one-shot isolation, script state,
script JSON Lines output, exact string preservation, and script limits. Session
help and completion behavior is also covered directly in
`tests/session/SessionTests.cpp`.

M0 decision: SDK-era formula commands remain private developer/demo tooling for
the public/private split. The initial protocol should not carry `tokens`,
`parse`, `validate`, `compile`, `evaluate`, `evaluate-host`, or
`host-functions`. Symbolic compatibility names may remain in the private CLI
during migration, but the durable public mental model should be ordinary
evaluation through a compatible `aleph-kernel`, not a permanent split between
SDK and symbolic evaluators.

### M1 - Add Protocol Model In The Current Repository

Before moving private files, add protocol and client model code while kernel
code is still local.

Deliverables:

- protocol encode/decode helpers;
- framed JSON reader and writer;
- malformed-message diagnostics;
- protocol documentation.

Focused tests:

- encode an `evaluate` request;
- decode an `evaluate` response;
- decode a diagnostic response;
- reject malformed JSON;
- reject invalid `Content-Length`;
- preserve request ids.

### M2 - Implement An In-Tree `aleph-kernel` Executable

Create an executable target that owns a `session::Session` and serves protocol
requests. This happens in the current repository first, before extraction.

Map methods to existing session behavior:

```text
evaluate  -> SessionOperation::evaluate
simplify  -> SessionOperation::simplify
fullForm  -> SessionOperation::full_form
help      -> SessionOperation::help
complete  -> SessionOperation::complete
packages  -> SessionOperation::discover_packs
reset     -> Session::reset
shutdown  -> clean process exit
```

Focused tests:

- start the process;
- send `initialize`;
- evaluate `1/2 + 1/3`;
- evaluate invalid syntax;
- evaluate `1/0`;
- call `help Expand`;
- call `complete Po`;
- reset session definitions.

### M3 - Add First-Party `KernelClient`

Create a client library that starts `aleph-kernel` as a child process and
communicates through the framed protocol. The private CLI is the first real
consumer. The same client model, or a small public subset of it, should later
be reused by the public notebook and agent clients.

Responsibilities:

- locate the kernel executable;
- start and stop the process;
- send framed requests;
- read framed responses;
- handle timeouts;
- handle early process exit;
- capture stderr or logs for diagnostics;
- send graceful shutdown where possible;
- report missing executable clearly.

Tests should use a fake kernel executable so protocol/client behavior can be
verified without invoking private kernel code.

### M4 - Migrate Private CLI Symbolic Commands To `KernelClient`

Change CLI symbolic commands so they no longer directly call `session::Session`
or symbolic helper functions.

Commands to migrate first:

- `symbolic-evaluate`
- `symbolic-simplify`
- `symbolic-fullform`
- `script --json`
- symbolic REPL mode
- `:help`
- `:complete`
- `:packs`
- `:reset`
- `:inspect` if retained

After this milestone, private CLI symbolic behavior should work through the
same process boundary the public notebook will use:

```text
aleph3_cli --kernel path/to/aleph-kernel symbolic-evaluate "Expand[(x+1)^2]"
```

Private developer/debug commands such as `tokens`, `parse`, SDK validation,
SDK compilation, and demo host-function evaluation may still call private code
inside the private repository. They are not part of the public app protocol.

### M5 - Prove A Public App Client Without Private Sources

Before physically splitting repositories, add an in-tree public-client build
mode or fixture that represents what the future public notebook repo may
contain:

- public `aleph_client` library;
- a small smoke-test app or notebook harness using `aleph_client`;
- protocol tests;
- fake-kernel tests.

It does not build:

- `aleph3_cli`;
- `aleph3_kernel`;
- private pack targets;
- symbolic evaluator tests;
- private kernel/session/pack tests.

Possible in-repo acceptance gate:

```text
cmake -S . -B build-public-client -DALEPH3_BUILD_PUBLIC_CLIENT_ONLY=ON
cmake --build build-public-client
ctest --test-dir build-public-client
```

The gate must pass without private source files and without compiling the CLI.

### M6 - Make Current Repo Private And Create Public App Repo

Make the current Aleph repository private only after the private CLI has proven
the `aleph-kernel` process boundary and the public-client fixture builds
without private sources.

The current private repository owns:

```text
include/expr
include/evaluator
include/kernel
include/symbols
include/normalizer
include/transforms
include/parser if private
include/syntax if parser is private
include/algebra
include/packs

src/expr
src/evaluator
src/kernel
src/symbols
src/transforms
src/session
src/algebra
src/packs
src/tooling/aleph3_cli.cpp
```

It also owns:

- `libaleph`;
- `aleph-kernel`;
- `aleph3_cli`;
- pack targets;
- kernel, session, pack, CLI, and real kernel-process tests.

Create a new public repository, provisionally `aleph-notebook`, containing:

- `aleph_client`;
- protocol documentation;
- notebook or app-client source;
- agent integration when that work becomes active;
- public examples and runtime setup documentation;
- fake-kernel tests.

### M7 - Private Kernel Build And Packaging

The private repository CI/CD should produce signed or otherwise provenance-
tracked runtime artifacts from the private source tree:

```text
aleph-kernel.exe
aleph3_cli.exe
```

Later it may also produce private libraries or separately packaged packs, but
the first public/private boundary requires the kernel executable and keeps the
CLI as a private diagnostic client.

Private artifact layout should include:

```text
aleph-kernel.exe
aleph3_cli.exe
LICENSES.txt
VERSION
```

Official product distributions may bundle the private kernel binary with the
public notebook or app client. The open public source should not contain the
private implementation or private CLI.

Private CI should at minimum:

- build `aleph-kernel` and the private CLI for the supported release targets;
- run kernel, session, pack, protocol, and private CLI integration tests;
- run a smoke test where the private CLI talks to the packaged
  `aleph-kernel`;
- produce versioned artifacts with checksums and dependency/license notices;
- record the supported protocol version and client compatibility range;
- fail the release if private symbols, debug artifacts, source paths, CI logs,
  or implementation headers would be published accidentally.

Public CI should not build the private kernel. It should build public app
clients against fake-kernel fixtures and, in private or release-bundle CI,
optionally smoke-test those public clients against the packaged private
`aleph-kernel` artifact.

### M8 - Public App Compatibility Tests With Fake Kernel

The public repository needs deterministic tests that do not require private
kernel code.

The fake kernel should support canned responses:

```text
initialize -> ok
evaluate "1/2 + 1/3" -> 5/6
evaluate "1/0" -> kernel.division_by_zero
help "Expand" -> canned help
complete "Ex" -> Expand
packages -> core-algebra, core-calculus
reset -> ok
```

These tests prove:

- process launch;
- protocol framing;
- error rendering;
- notebook/app request orchestration;
- public diagnostic and result rendering.

### M9 - Private Integration Tests Against Real Kernel

The private repository should test the private CLI against the real private
kernel:

```text
aleph3_cli --kernel ./aleph-kernel symbolic-evaluate "1/2 + 1/3"
aleph3_cli --kernel ./aleph-kernel symbolic-evaluate "Expand[(x+1/3)^2]"
aleph3_cli --kernel ./aleph-kernel symbolic-evaluate "Det[{{1,2},{3,4}}]"
aleph3_cli --kernel ./aleph-kernel symbolic-evaluate "1/0"
```

This proves the CLI and kernel remain compatible through the same process
boundary used by the public notebook. Public app integration tests against the
real private kernel can run in private CI or release smoke tests when a product
bundle is assembled.

### M10 - Documentation And Release Rules

Public documentation should state:

- mathematical execution requires a compatible `aleph-kernel`;
- the kernel protocol version is public;
- the private kernel binary supplies Aleph semantics;
- the public notebook or app client does not implement mathematical semantics;
- the CLI is private developer tooling unless a separate public CLI is later
  introduced.

Private documentation should state:

- how to build `aleph-kernel`;
- how to build and use the private CLI;
- supported protocol version;
- compatibility matrix with public client versions;
- release artifact checklist.

Release rule:

```text
Public repository artifacts must not include private headers, private source,
debug symbols exposing private implementation, CI logs with private paths, or
bundled private binaries unless explicitly intended for an official product
release.
```

## Verification Strategy

Public repository verification:

- protocol encode/decode tests;
- framed IO tests;
- fake-kernel product-client tests;
- missing-kernel diagnostics;
- public app-client build with private sources disabled.

Private repository verification:

- existing kernel, pack, session, and symbolic behavior tests;
- `aleph-kernel` protocol tests;
- real CLI-to-kernel integration tests;
- packaging smoke test proving a public notebook or app client can find and
  use the private kernel executable.

## Completion Criteria

The public app / private kernel split is complete when:

- public app-client code includes no private kernel, parser, expression,
  evaluator, exact arithmetic, CLI, or pack headers;
- public app-client build and tests pass without private source files;
- private `aleph-kernel` passes kernel/session/pack tests;
- private CLI plus private `aleph-kernel` pass real integration tests;
- public app client plus private `aleph-kernel` pass product-bundle smoke
  tests;
- protocol versioning and incompatible-version diagnostics exist;
- documentation accurately distinguishes public source from private semantics;
- release checks prevent accidental publication of private implementation
  artifacts.
