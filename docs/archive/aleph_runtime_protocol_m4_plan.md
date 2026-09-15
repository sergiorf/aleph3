# Aleph Runtime Protocol M4 Plan

## Status

Implemented. This is the completed implementation record for M4 from
[Public App / Private Kernel Split](../public_private_cli_split_plan.md): private
CLI migration through the M3 runtime client.

M4 started after M3 delivered a process-launching runtime client with
fake-runtime tests, real-runtime smoke tests, lookup behavior, timeout
handling, and stable lifecycle diagnostics.

## Goal

Route ordinary symbolic CLI behavior through the runtime client so the first
real consumer proves the process boundary before notebook work begins.

The slice succeeds when `aleph3_cli` can run ordinary symbolic REPL and script
workflows through `aleph-runtime` with the same user-visible outputs,
diagnostics, reset behavior, help, completion, and package discovery as the
current direct `session::Session` path.

## Roadmap Alignment

- Advances milestone 4, **Private CLI migration**, in the public/private split
  plan.
- Uses a mature internal tool to test the runtime boundary before
  notebook-lite or public notebook work depends on it.
- Keeps the CLI private first-party tooling.
- Does not change symbolic semantics, supported input, SDK behavior, notebook
  behavior, or repository visibility.

## Ownership

The CLI remains private tooling:

```text
src/tooling/aleph3_cli.cpp
src/tooling/
tests/tooling/
```

The migrated ordinary symbolic path may depend on `aleph_client` and launch
`aleph-runtime`. It must not duplicate session behavior or add CLI-only
semantic fallbacks.

Keep these commands private developer tools on their existing direct paths
unless a separate product design promotes them:

- parser-token dumps;
- parse-tree dumps;
- SDK validation and compile tooling;
- demo host-function commands;
- private inspection commands not present in the runtime protocol.

## Chosen Design

Add an internal CLI runtime mode that uses the M3 client for protocol-supported
operations. The mode may become the default for ordinary symbolic commands
after conformance tests prove parity. During migration, a development fallback
to the direct session path may exist only for diagnosis and must not mask
runtime-client failures in tests.

Ordinary protocol-backed behavior includes:

- one-shot symbolic evaluation;
- symbolic simplification;
- full-form rendering;
- REPL expression evaluation;
- `:help` and focused help;
- `:complete`;
- `:packs`;
- `:reset`;
- script execution and JSON Lines output where protocol response data can
  preserve the current public shape.

`inspect` remains on the private direct path because it is a session/CLI
operation and not an M1/M2 protocol method. Adding it to the runtime boundary
requires a focused protocol change plan.

## Non-Goals

- public CLI productization;
- notebook-lite or graphical notebook work;
- physical repository movement;
- packaging or release checks beyond what CLI tests need;
- new symbolic semantics or protocol methods except a separately approved
  private inspection slice;
- changing SDK validation, host-function demos, parser tokens, or parse-tree
  tooling;
- concurrent requests, streaming output, cancellation, or runtime pools.

## Implementation Tasks

### 1. Baseline And CLI Surface Audit

List every CLI command and REPL command, then classify it as protocol-backed,
direct-private, or requires a protocol decision.

Completion criteria:

- ordinary symbolic commands are mapped to existing protocol methods;
- private developer commands are explicitly left on direct paths;
- any unsupported command has a documented diagnostic or migration decision;
- current CLI tests are run or baseline failures are recorded.

Focused verification:

```text
cmake --build build --config Release --target aleph3_cli aleph3_sdk_tests aleph_client_tests aleph-runtime
ctest --test-dir build -C Release -R "aleph3_sdk_tests|aleph_client_tests" --output-on-failure
```

### 2. Runtime-Backed One-Shot Commands

Route one-shot symbolic evaluation, simplification, and full-form operations
through the runtime client.

Completion criteria:

- output text matches the current direct session path for representative
  success cases;
- protocol and lifecycle failures produce deterministic CLI diagnostics and
  exit codes;
- `--runtime <path>` or the chosen internal equivalent participates in the M3
  lookup order;
- exact outputs remain strings and are not coerced through host numeric types.

Focused tests:

- `symbolic-evaluate "1/2 + 1/3"` prints `5/6`;
- `symbolic-simplify "0 + x"` prints `x`;
- `symbolic-fullform "f[x]"` preserves full-form output;
- missing runtime reports a stable failure.

### 3. Runtime-Backed REPL Discovery And Reset

Route REPL expression evaluation and protocol-supported commands through the
runtime client.

Completion criteria:

- one REPL runtime process preserves definitions across inputs;
- `:reset` clears session-local definitions through the runtime;
- `:help`, focused help, `:complete`, and `:packs` match session-backed output
  for the supported surface;
- direct private commands remain available only where intentionally retained.

Focused tests:

- REPL evaluates `a = 2`, `a`, `:reset`, `a` with the current output shape;
- `:help Factor`, `:complete Pol`, and `:packs` produce existing information;
- runtime lifecycle failure during REPL startup exits cleanly.

### 4. Runtime-Backed Scripts

Route `aleph3_cli script` and `script --json` through one runtime process.

Completion criteria:

- script state flows across lines through the runtime session;
- failures do not stop later lines;
- exit code behavior matches the existing script contract;
- JSON Lines output preserves the current field names, line numbers, status,
  canonical output strings, and diagnostics.

Focused tests:

- stateful script parity with the existing CLI tests;
- JSON Lines parity for success, failure, and large exact output strings;
- oversized script and line limits remain enforced before runtime evaluation.

### 5. Conformance And Fallback Removal

Compare runtime-backed CLI behavior with the direct session baseline and remove
or hide migration fallbacks that would conceal boundary failures.

Completion criteria:

- focused CLI tests exercise the runtime path by default for ordinary symbolic
  behavior;
- direct session helpers remain only where private developer commands need
  them;
- runtime lookup, missing runtime, incompatible version, timeout, and malformed
  output failures are covered from the CLI surface;
- no CLI-owned semantic parsing or simplification is introduced.

Focused verification:

```text
cmake --build build --config Release --target aleph3_cli aleph_client_tests aleph-runtime aleph_runtime_protocol_tests aleph3_sdk_tests
ctest --test-dir build -C Release -R "aleph_client_tests|aleph_runtime_protocol_tests|aleph3_sdk_tests" --output-on-failure
```

### 6. Documentation

Update current-behavior docs after implementation:

- `docs/public_private_cli_split_plan.md` marks M4 implemented and makes M5,
  private notebook-lite, the next split milestone;
- `docs/architecture.md` records the private CLI as a runtime-client consumer;
- `docs/sdk/build_and_targets.md` records target dependencies and test
  coverage;
- `docs/manual/sessions-cli-and-notebook.md` explains that ordinary CLI
  symbolic behavior uses the runtime boundary while developer commands remain
  private direct tools where applicable;
- CLI help documents runtime path options and lifecycle failures when they are
  user-visible.

## Completion Criteria

M4 is complete when:

- ordinary symbolic CLI behavior runs through the M3 runtime client;
- REPL, script, help, completion, packages, reset, and one-shot symbolic
  commands preserve current user-visible behavior;
- lifecycle and protocol failures have deterministic CLI diagnostics and exit
  codes;
- private developer commands remain explicitly private and do not leak into
  the public runtime protocol by accident;
- focused and affected broader tests pass or unrelated failures are recorded;
- documentation identifies private notebook-lite as the next boundary
  rehearsal before public notebook work.

## Next Slice

After M4, proceed to M5: build a private notebook-lite harness or app over the
runtime client to rehearse notebook workflows inside the internal repository.
