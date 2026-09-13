# Public/Private CLI Split Plan

## Status

This is a proposed implementation plan. It records the intended first stage of
a public/private repository split for Aleph3, scoped to the CLI and a local
kernel process. It does not describe shipped behavior until the milestones are
implemented and the owning specifications are updated.

This plan complements the practical guidance in
[IP and Repository Strategy](ip_and_repo_strategy.md). It is engineering
guidance, not legal advice.

## Goal

Refactor Aleph3 so the open-source CLI can be built, tested, and distributed
without containing private kernel, parser, evaluator, exact arithmetic, or math
pack implementations.

The public CLI communicates with a separately built private `aleph-kernel`
executable through a versioned local JSON protocol.

The initial product boundary is:

```text
open-source CLI
        |
        | framed JSON over stdin/stdout
        v
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
- public C++ kernel headers;
- semantic changes to Aleph expressions, evaluation, exact arithmetic, or
  supported mathematics.

## Public And Private Ownership

The public repository should contain:

- CLI application source and presentation code;
- public kernel protocol and client types;
- process-launching client code;
- notebook document format and later notebook UI code when that work resumes;
- generic agent framework and provider adapters when that work becomes active;
- protocol, CLI, and fake-kernel tests;
- public documentation, examples, and compatibility notes.

The private repository should contain:

- `libaleph`;
- parser and symbolic lowering;
- `Expr` and the expression model;
- evaluator and runtime;
- exact integer, rational, real, and complex implementations;
- symbol/session state;
- function and package registration internals;
- simplification, rewriting, assumptions, and diagnostics implementation;
- Algebra, Calculus, Linear Algebra, and future domain pack implementations;
- the real `aleph-kernel` executable;
- advanced Aleph-specific Agent reasoning if it becomes a product
  differentiator.

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

The public repository should evolve toward:

```text
apps/
  cli/

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
  cli/
  protocol/
  fake_kernel/

docs/
  public_private_cli_split_plan.md
  kernel_protocol.md
```

The private repository should evolve toward:

```text
libaleph/
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

apps/
  aleph-kernel/
```

The first physical moves should happen only after the public CLI has stopped
including private headers.

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

Minimum CLI-stage methods:

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

The public repository may define plain transport-facing types such as:

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

The CLI should fail clearly when the kernel protocol version is incompatible.

## Kernel Location

The public CLI should find `aleph-kernel` in this order:

1. explicit `--kernel <path>` argument;
2. `ALEPH_KERNEL_PATH`;
3. same directory as the CLI executable;
4. `PATH` lookup.

If no compatible kernel is found, the CLI should report a clear diagnostic and
avoid implying that public CLI code can perform private mathematical
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

Decide whether trusted SDK formula features remain public or are routed through
the private kernel boundary. For the IP split, symbolic CLI behavior should
move through `aleph-kernel`; broader SDK cleanup can be deferred unless it
blocks the boundary.

#### M0 Boundary Audit

The initial public command name remains `aleph3_cli`. A later product rename
may introduce `aleph` or another shorter launcher after the notebook and
distribution shape are clearer; the repository split should not combine a
public/private architecture change with a command rename.

The private executable name is `aleph-kernel`.

The public user-facing direction is a small CLI whose normal evaluation,
script, REPL, help, completion, package discovery, and reset behavior routes
through the kernel process. The protocol should be intent-shaped and stable,
with plain textual source and result representations, structured diagnostics,
and capability metadata. It should not expose parser tokens, SDK IR nodes,
`Expr`, evaluator internals, exact arithmetic storage, or pack registration
implementation details. AI-facing and natural-language-friendly behavior
belongs in help text, examples, documentation, and later higher-level
commands; the process protocol itself stays deterministic and typed.

Current command inventory:

| Command or mode | Current implementation path | Split classification |
| --- | --- | --- |
| no arguments / `repl` | starts the interactive CLI; default mode is symbolic when `ALEPH3_HAS_SYMBOLIC_ENGINE` is enabled | public shell that should use `KernelClient` for symbolic work |
| bare CLI expression | falls through to `run_default_expression`, symbolic when available | public evaluation surface; route through `aleph-kernel` |
| `help`, `--help`, `-h` | static CLI presentation text plus symbolic help in the REPL | public presentation; symbolic help entries route through `aleph-kernel` |
| `examples` | static CLI examples | public presentation; examples must avoid claiming private implementation is public |
| `script [--json] <path>` | owns file reading, limits, JSON Lines rendering, and a `session::Session` for stateful evaluation | public command; evaluation/reset state must route through `aleph-kernel` while file IO and JSON Lines formatting remain public |
| `host-functions` | prints demo SDK host function docs from `tooling/DemoHostFunctions` | SDK-era demo command; transitional and not part of the first public kernel protocol |
| `tokens <formula>` | calls `frontend::Lexer` and prints token internals | trusted-frontend/debug leftover; do not carry into the first public protocol |
| `parse <formula>` | calls `frontend::Parser` and prints SDK IR internals | trusted-frontend/debug leftover; do not carry into the first public protocol |
| `validate <formula>` | calls `sdk::Engine::validate` with an empty schema | SDK-era command; transitional unless a later public SDK developer tool is explicitly kept |
| `compile <formula>` | calls `sdk::Engine::compile` and reports compile success | SDK-era command; transitional unless a later public SDK developer tool is explicitly kept |
| `evaluate [--var ...] <formula>` | calls `sdk::Engine::compile` and `evaluate` with CLI bindings | SDK-era command; transitional unless a later public SDK developer tool is explicitly kept |
| `evaluate-host [--var ...] <formula>` | registers demo host functions, then calls SDK compile/evaluate | SDK-era demo command; transitional and not part of the first public kernel protocol |
| `symbolic-evaluate <expr>` | calls symbolic CLI helpers, which use private symbolic behavior | compatibility command; route through `aleph-kernel` |
| `symbolic-simplify <expr>` | calls symbolic CLI helpers, which use private symbolic behavior | compatibility command; route through `aleph-kernel` |
| `symbolic-fullform <expr>` | calls symbolic CLI helpers, which use private symbolic behavior | compatibility command; route through `aleph-kernel` |

Current REPL command inventory:

| REPL command | Current implementation path | Split classification |
| --- | --- | --- |
| bare input | uses active mode, symbolic by default when available | public evaluation surface; route through `aleph-kernel` |
| `:help [name-or-prefix]` | static command help or `SessionOperation::help` | public; symbolic entries route through `aleph-kernel` |
| `:examples` | static examples | public presentation |
| `:mode [sdk|symbolic]` | switches between local SDK and symbolic execution modes | transitional; the public split should avoid making dual evaluators a lasting product concept |
| `:host-functions` | static demo SDK host function docs | SDK-era demo command; transitional |
| `:tokens`, `:parse`, `:validate`, `:compile`, `:evaluate`, `:evaluate-host` | same local lexer/parser/SDK paths as top-level commands | transitional; do not include in the first public kernel protocol |
| `:symbolic-evaluate`, `:symbolic-simplify`, `:symbolic-fullform` | symbolic helper paths | compatibility commands; route through `aleph-kernel` |
| `:inspect <expr>` | `SessionOperation::inspect` | public diagnostic command if retained; route through `aleph-kernel` |
| `:packs` | `SessionOperation::discover_packs` | public discovery command; route through `aleph-kernel` |
| `:complete <prefix>` | `SessionOperation::complete` | public discovery command; route through `aleph-kernel` |
| `:reset` | `session::Session::reset` | public session lifecycle command; route through `aleph-kernel` |
| `:quit`, `:exit` | local REPL control | public shell behavior; stays in CLI |

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
and reset behind `aleph-kernel`. It should not attempt to preserve public
access to token streams, parser trees, SDK IR, or demo host functions through
the kernel protocol. If those SDK-era developer tools remain useful, they need
a separate public SDK-tooling decision after the CLI/kernel boundary is stable.

Outputs to preserve during the compatibility phase:

- plain text result rendering for bare CLI expressions and `symbolic-*`
  commands;
- REPL prompt and command behavior where practical, especially bare input,
  `:help`, `:complete`, `:packs`, `:reset`, `:inspect`, and `:quit`;
- script continuation after failed lines, line-numbered diagnostics, exit code
  `2` when any line fails, and line-size failure exit code `3`;
- `script --json` JSON Lines fields `schema_version`, `line`, `source`, `ok`,
  `output`, and `diagnostics`, with exact outputs preserved as strings;
- deterministic missing-kernel and incompatible-protocol diagnostics once the
  process boundary exists.

Existing behavior evidence is concentrated in `tests/tooling/Aleph3CliTests.cpp`.
It covers bare expression evaluation, REPL meta commands and help, mode
switching, pack-backed matrix and calculus examples, session state, inspection,
completion, reset, cleanup precedence, one-shot isolation, script state,
script JSON Lines output, exact string preservation, and script limits. Session
help and completion behavior is also covered directly in
`tests/session/SessionTests.cpp`.

M0 decision: SDK-era formula commands are transitional for the public/private
split. The initial protocol should not carry `tokens`, `parse`, `validate`,
`compile`, `evaluate`, `evaluate-host`, or `host-functions`. Public symbolic
compatibility names may remain during migration, but the durable public mental
model should be ordinary evaluation through a compatible `aleph-kernel`, not a
permanent split between SDK and symbolic evaluators in the CLI.

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

### M3 - Add Public `KernelClient`

Create a CLI-facing client that starts `aleph-kernel` as a child process and
communicates through the framed protocol.

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

Tests should use a fake kernel executable so public CLI tests do not require
private kernel code.

### M4 - Migrate CLI Symbolic Commands To `KernelClient`

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

After this milestone, CLI symbolic behavior should work through the process
boundary:

```text
aleph3_cli --kernel path/to/aleph-kernel symbolic-evaluate "Expand[(x+1)^2]"
```

### M5 - Support A Public CLI Build Without Private Kernel Sources

Add a build option such as:

```text
ALEPH3_BUILD_PRIVATE_KERNEL=OFF
```

When private kernel sources are disabled, the public repository builds:

- `aleph3_cli`;
- public `aleph_client` library;
- protocol tests;
- fake-kernel tests.

It does not build:

- `aleph3_kernel`;
- private pack targets;
- symbolic evaluator tests;
- private kernel/session/pack tests.

Acceptance gate:

```text
cmake -S . -B build-public -DALEPH3_BUILD_PRIVATE_KERNEL=OFF
cmake --build build-public
ctest --test-dir build-public
```

The gate must pass without private source files.

### M6 - Extract Private Code

Move private implementation to the private repository only after the public
CLI has stopped including private headers.

Private candidates include:

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
```

The private repository owns:

- `libaleph`;
- `aleph-kernel`;
- pack targets;
- kernel, session, and pack tests.

The public repository keeps:

- CLI source;
- `aleph_client`;
- protocol documentation;
- CLI documentation;
- fake-kernel tests.

### M7 - Private Kernel Build And Packaging

The private repository should produce:

```text
aleph-kernel.exe
```

Later it may also produce private libraries or separately packaged packs, but
the first public/private boundary only requires the kernel executable.

Private artifact layout should include:

```text
aleph-kernel.exe
LICENSES.txt
VERSION
```

Official product distributions may bundle the private kernel binary with the
public CLI. The open public source should not contain the private
implementation.

### M8 - Public Compatibility Tests With Fake Kernel

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

- CLI argument parsing;
- process launch;
- protocol framing;
- error rendering;
- JSON output mode;
- REPL command plumbing where practical.

### M9 - Private Integration Tests Against Real Kernel

The private repository should test the public CLI against the real private
kernel:

```text
aleph3_cli --kernel ./aleph-kernel symbolic-evaluate "1/2 + 1/3"
aleph3_cli --kernel ./aleph-kernel symbolic-evaluate "Expand[(x+1/3)^2]"
aleph3_cli --kernel ./aleph-kernel symbolic-evaluate "Det[{{1,2},{3,4}}]"
aleph3_cli --kernel ./aleph-kernel symbolic-evaluate "1/0"
```

This proves the public CLI and private kernel remain compatible without
duplicating semantics in the public repository.

### M10 - Documentation And Release Rules

Public documentation should state:

- CLI source is open;
- mathematical execution requires a compatible `aleph-kernel`;
- the kernel protocol version is public;
- the private kernel binary supplies Aleph semantics;
- the CLI does not implement mathematical semantics.

Private documentation should state:

- how to build `aleph-kernel`;
- supported protocol version;
- compatibility matrix with public CLI versions;
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
- fake-kernel CLI tests;
- missing-kernel diagnostics;
- public build with private sources disabled.

Private repository verification:

- existing kernel, pack, session, and symbolic behavior tests;
- `aleph-kernel` protocol tests;
- real CLI-to-kernel integration tests;
- packaging smoke test proving a built public CLI can find and use the private
  kernel executable.

## Completion Criteria

The CLI split is complete when:

- public CLI code includes no private kernel, parser, expression, evaluator,
  exact arithmetic, or pack headers;
- public build and tests pass without private source files;
- private `aleph-kernel` passes kernel/session/pack tests;
- public CLI plus private `aleph-kernel` pass real integration tests;
- protocol versioning and incompatible-version diagnostics exist;
- documentation accurately distinguishes public source from private semantics;
- release checks prevent accidental publication of private implementation
  artifacts.
