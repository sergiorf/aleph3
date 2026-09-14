# Aleph Runtime Protocol M5 Plan

## Status

This is the planned detailed implementation plan for M5 from
[Public App / Private Kernel Split](public_private_cli_split_plan.md): a
private notebook-lite consumer over the runtime client.

M5 should start only after M4 proves ordinary private CLI behavior through the
runtime boundary.

## Goal

Build a private notebook-lite harness or small internal application that
rehearses notebook workflows over the same runtime client the later public
notebook will use.

The slice succeeds when notebook-shaped create/edit/evaluate/save/reopen and
`Run All` workflows exercise `aleph-runtime` through the client boundary,
without starting the public notebook repository and without adding UI-owned
semantic behavior.

## Roadmap Alignment

- Adds an internal rehearsal stage before the public notebook product repo.
- Uses the runtime protocol as the shared semantic and agent-facing boundary.
- Keeps the current repository as the private place to stabilize workflow,
  diagnostics, runtime lookup, and document interaction.
- Does not make notebook-lite the public product surface.

## Ownership

Notebook-lite is private first-party tooling or an internal harness. It may use
the existing notebook document core and the runtime client:

```text
include/notebook/
src/notebook/
src/tooling/ or a future private app directory
tests/notebook/
tests/tooling/
```

Notebook-lite may own document interaction, source editing fixtures,
presentation choices, file operations, and workflow orchestration. It must not
parse expressions into private semantic objects, simplify results, reinterpret
diagnostics, or bypass the runtime protocol for ordinary evaluation.

Agent-facing automation for notebook workflows should use the same runtime
boundary or an explicitly versioned internal extension. Do not add a separate
agent semantic protocol in M5.

## Chosen Design

Start with the smallest private notebook-lite surface that proves behavior. A
command-line harness over notebook documents is acceptable if it exercises the
same product workflows more cheaply than a GUI. A lightweight internal GUI may
follow only after the harness has stable runtime-backed behavior.

The first notebook-lite path should support:

- create or open a bounded notebook document;
- edit or replace input/text cell source through deterministic commands or
  fixtures;
- run one input cell through the runtime client;
- run all input cells through a clean runtime session;
- save and reopen the document;
- preserve and clear cached generated results;
- display canonical text, request status, and structured diagnostics;
- call runtime help, completion, packages, reset, initialize, and shutdown;
- show runtime lookup and lifecycle failures in a user-understandable way.

Use the existing headless notebook JSON format unless a focused persistence
plan changes it. Notebook-lite should reveal gaps in the protocol or notebook
core, not paper over them with private semantics.

## Non-Goals

- public notebook repository creation;
- public GUI product launch;
- toolkit decision unless a separate measured spike is approved;
- rich mathematical typesetting;
- plotting, export, collaboration, cloud execution, marketplace, or accounts;
- cancellation, streaming, concurrent cell execution, automatic restart, or
  crash recovery unless a prior runtime/client slice has specified them;
- new symbolic functions, parser syntax, simplifications, or pack behavior;
- separate agent semantic protocol.

## Implementation Tasks

### 1. Workflow Contract And Fixture

Define the exact notebook-lite workflow and representative document fixture.

Completion criteria:

- fixture covers exact arithmetic, assignments, cleanup/reset, rewriting or
  assumptions, polynomial algebra, focused differentiation when available, and
  one deliberate diagnostic;
- every evaluated cell uses the runtime client;
- cached outputs remain display data and are replaced by runtime-backed runs;
- gaps are classified as protocol, notebook-core, session, or presentation
  work.

Focused tests:

- create the fixture in memory;
- run all cells through a fake runtime and assert request ordering;
- run all cells through the real runtime and assert canonical outputs where
  current behavior is stable.

### 2. Runtime-Backed Notebook Runner

Add or adapt a runner that evaluates notebook cells through one runtime-client
session rather than constructing `session::Session` directly.

Completion criteria:

- `Run All` starts from a clean runtime session;
- individual cell execution preserves later session state only within the
  active runtime process;
- generated results store canonical text and diagnostics from protocol
  responses;
- failed cells do not stop later cells;
- runtime lifecycle failures are represented as notebook diagnostics without
  changing source cells.

Focused tests:

- definitions flow across cells in one run;
- rerun starts clean;
- deliberate failure records diagnostics and later cells still run;
- runtime timeout or exit produces a notebook-level failure record.

### 3. Persistence Rehearsal

Exercise save/reopen around runtime-backed generated results.

Completion criteria:

- notebook-lite saves the existing bounded JSON v1 format;
- reopening never evaluates source;
- cached outputs are visibly or structurally marked as cached when they have
  not been reproduced in the current session;
- clearing cached results does not change source cells;
- malformed or oversized documents keep current notebook diagnostics.

Focused tests:

- run, save, reopen, inspect cached results;
- clear cached results, save, reopen, confirm caches are gone;
- malformed document load fails without starting runtime evaluation.

### 4. Discovery And Diagnostics Surface

Exercise help, completion, package discovery, reset, and lifecycle failures
from notebook-lite.

Completion criteria:

- help and completion use runtime protocol data;
- package discovery is stable and matches CLI/runtime behavior;
- reset clears definitions and cached running state consistently;
- missing runtime, incompatible version, timeout, malformed output, and early
  exit are visible as structured notebook-lite diagnostics.

Focused tests:

- request help for `Factor`;
- complete a prefix that includes pack and session-local names;
- reset removes session-local completions;
- fake runtime failure modes produce expected diagnostics.

### 5. Internal UI Or Harness Polish

Keep the first surface intentionally modest and deterministic.

Completion criteria:

- keyboard-only or command-only workflow can create, edit, run, save, reopen,
  reset, and inspect diagnostics;
- output presentation has canonical text fallback;
- no in-product text claims unsupported capabilities;
- notebook-lite remains marked private/internal in docs and help.

Focused verification:

```text
cmake --build build --config Release --target aleph3_notebook_tests aleph_client_tests aleph-runtime
ctest --test-dir build -C Release -R "aleph3_notebook_tests|aleph_client_tests|aleph_runtime_protocol_tests" --output-on-failure
```

### 6. Documentation

Update current-behavior docs after implementation:

- `docs/public_private_cli_split_plan.md` marks M5 implemented and makes the
  public-client fixture or extraction proof the next split milestone;
- `docs/architecture.md` records private notebook-lite as an internal runtime
  client consumer;
- `docs/notebook_mvp_design.md` distinguishes private notebook-lite rehearsal
  from the later public notebook product;
- `docs/manual/sessions-cli-and-notebook.md` documents available notebook-lite
  commands or harness behavior if user-visible to developers;
- `docs/sdk/build_and_targets.md` records targets and tests.

## Completion Criteria

M5 is complete when:

- private notebook-lite exercises create/edit/evaluate/save/reopen/`Run All`
  workflows through the runtime client;
- notebook-lite uses no private semantic shortcut for ordinary evaluation;
- help, completion, package discovery, reset, diagnostics, and runtime
  lifecycle failures are tested through fake and real runtimes where
  appropriate;
- the public notebook remains unstarted as a repository/product effort;
- docs clearly describe notebook-lite as an internal rehearsal tool and record
  the next public extraction proof;
- focused and affected broader tests pass or unrelated failures are recorded;
- `git diff --check` passes and the final diff has been reviewed.

## Next Slice

After M5, prove the public-client fixture or extraction build: protocol,
client, notebook-facing code, and fake-runtime tests build without private
kernel, CLI, pack, parser, session, or semantic implementation targets.
