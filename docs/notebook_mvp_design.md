# Notebook MVP Design

## Product Scope

Aleph3's intended main product is a lightweight local symbolic notebook for
developers, engineers, students, and technical users who want a fast native
workspace without adopting a Python/Jupyter stack or a commercial CAS. It is
a focused product, not a claim of broad CAS or notebook-platform parity.

The kernel remains the semantic asset, the session supplies interactive state,
and the CLI remains the scripting and diagnostic fallback. Make the free or
low-friction notebook useful before investing substantially in paid packs,
hosted services, or provider-backed features.

## Status and Goal

This document is the proposed contract for the first Aleph3 notebook. The
headless document, `Run All`, JSON persistence, and cached-result clearing
slices described below are implemented; the GUI and remaining behavior are
planned rather than shipped.

The MVP succeeds when a user can launch a local application, create and edit a
document, evaluate supported symbolic input through one session, understand
results or failures, save the document, reopen it, and run bundled examples.

The product name and GUI toolkit remain open. The first packaged distribution
target is Windows-first unless the toolkit decision records a different
measured result. The physical v1 encoding is bounded UTF-8 JSON.

## Ownership

| Concern | Owner |
| --- | --- |
| expression meaning, exactness, evaluation, and budgets | kernel |
| interactive definitions and request/result lifecycle | session |
| domain algorithms and symbols | registered packs |
| cell ordering, editing, documents, display, and file interaction | notebook |
| trusted host schemas, policy, and host-value conversion | SDK |

The notebook must not add parser rules, simplifications, fallback behavior, or
pack implementations. Missing shared behavior is a kernel, session, or pack
contract gap rather than permission for a widget-local workaround.

## Document Model

A document has:

- a stable format identifier and version;
- an ordered list of cells with stable document-local identifiers;
- source content and a declared cell kind;
- optional presentation metadata that does not affect evaluation;
- optional cached output marked with the producer version;
- document-level metadata limited to portable product concerns.

The MVP cell kinds are `input`, `text`, and `output`. An output is associated
with the input request that produced it. Unknown future cell fields should be
preserved when practical, while unknown required cell kinds or incompatible
major versions fail with a structured load diagnostic.

Notebook files must not contain native pointers, opaque evaluator objects,
provider secrets, API tokens, or an implicit claim that cached output is still
valid. The physical encoding may be JSON, an archive, or another inspectable
format. Unknown optional object fields are accepted and ignored in v1; unknown
cell kinds and incompatible versions fail explicitly.

## Evaluation Lifecycle

For the first release:

1. opening or creating a document creates an isolated session;
2. running an input cell submits its source and an explicit operation;
3. the session parses and evaluates within per-request budgets;
4. the notebook renders the structured result or diagnostic;
5. successful definitions affect later requests in that session;
6. editing an earlier cell does not silently re-evaluate dependent cells;
7. `Run All` starts from a clean session and evaluates input cells in order.

On reopen, `Run All` is the authoritative way to reconstruct session state.
Persisted output is a display cache and must be visibly distinguishable when
it has not been reproduced by the current kernel/product version.

Cancellation and restart or reset are part of the graphical MVP contract, but
they must cooperate with kernel budgets and preserve session integrity.
Forcefully terminating arbitrary kernel work is not an MVP promise until
recovery and state integrity are specified.

## Results, Display, and Diagnostics

The session result should carry enough structure for the notebook to present:

- canonical plain text, always available for copying;
- optional structured mathematical display with a plain-text fallback;
- diagnostic code, severity, message, and source location where available;
- request status and relevant budget exhaustion information;
- optional inspection metadata through a separate, non-mutating operation.

The renderer may choose typography and layout but may not reinterpret an
expression. A display failure falls back to canonical text rather than changing
or re-evaluating the result.

## Save, Load, and Recovery

- Save uses an atomic replace strategy where the platform permits it.
- A failed save leaves the previous valid file intact and reports the path and
  structured failure.
- Load validates format/version and bounded sizes before constructing the full
  document in memory.
- Parse or compatibility failure does not partially mutate the open document.
- Autosave and crash recovery are desirable after the basic save/reopen loop;
  they are not required for the first vertical slice.
- Notebook files are untrusted input. Loading a file never evaluates cells.

## Example Gallery

Bundled examples are read-only templates copied into a new user document. Each
example must be exercised by an automated test or the built executable and use
only the documented supported subset. The first gallery should demonstrate:

- exact arithmetic;
- assignments and session state;
- rewriting and assumptions;
- polynomial expansion, factorization, and exact division;
- diagnostics for one deliberately unsupported or invalid expression.

Calculus, trigonometric identity simplification, code generation, and paid-pack
examples wait until their shared contracts are implemented and tested.

## Explicit Non-Goals

The MVP does not require cloud accounts, collaboration, arbitrary HTML or
script execution, plugin installation, AI services, PDF/HTML export, broad CAS
coverage, or a marketplace. Markdown may begin as a safe text subset. Plotting
requires its own bounded data contract and is not part of the first
create/evaluate/save/reopen slice.

## Toolkit Spike and Decision Gate

Build disposable Qt and webview/native-wrapper spikes against the same fake or
real session fixture. Compare:

- application and packaged size;
- cold startup and first-evaluation latency;
- multiline editing, keyboard navigation, accessibility, and clipboard use;
- mathematical display and plain-text fallback;
- Windows, Linux, and later macOS packaging effort;
- licensing and redistribution obligations;
- implementation complexity for cells, diagnostics, and atomic persistence.

Record the decision in [Architecture](architecture.md) before production UI
code depends on a toolkit. Dear ImGui, wxWidgets, and custom UI remain fallback
options if the measured constraints reject both primary candidates.

## Delivered First Implementation Slice

The first slice is a headless notebook document core and deterministic session
runner. It deliberately precedes persistence and GUI work so every toolkit
spike consumes the same tested model instead of defining its own cells or
execution lifecycle.

### Observable Outcome

A test or small development harness can construct a document containing text
and input cells, run all inputs through a fresh session in document order, and
inspect one canonical result record per input. Definitions created by an
earlier input are visible to later inputs during that run, while separate
documents and repeated `Run All` operations do not inherit stale session
state.

This is an internal product foundation, not yet a notebook application or a
user-visible release. Its behavior is covered by `aleph3_notebook_tests`.

### Slice Contract

- Add a GUI-independent `aleph3_notebook_core` target above the session and
  kernel. It owns document, cell, generated-result, and runner types; it links
  to shared session behavior and contains no evaluator or parser logic.
- A document has the format identity and version fields needed by later
  persistence plus an ordered cell collection. Physical serialization is not
  part of this slice.
- Source cells are `input` or `text`. Evaluation results are generated records
  associated with an input-cell identifier rather than editable source.
  Presentation code may later expose those records as output cells.
- Cell identifiers are opaque, non-empty, document-local strings. Creation
  receives an identifier generator so tests remain deterministic. Duplicate
  identifiers are rejected by document validation.
- `Run All` creates a new `session::Session`, clears all prior generated
  results, skips text cells, and submits every input cell in order using
  `SessionOperation::evaluate`.
- Each attempted input produces exactly one generated record containing the
  source-cell identifier, success status, canonical plain text when present,
  and the diagnostics currently returned by the session. A failed input does
  not stop later cells from running.
- The runner does not interpret output text, synthesize diagnostics, retry a
  failed expression, or mutate input source.

Rich display nodes, diagnostic severity and source spans require shared
session contracts that do not exist yet. The first slice preserves the current
diagnostic code and message without pretending those richer fields are
available.

### Ordered Tasks and Gates

1. **Model and validation.** Introduce the notebook-core target and document
   types with stable ordering, opaque identifiers, input/text source kinds,
   generated results, format identity, and version. Add tests for construction,
   ordering, duplicate or empty identifiers, and unsupported format versions.
2. **Session-backed runner.** Implement deterministic `Run All` over a newly
   constructed session. Add tests proving definition flow within one run,
   clean reruns after source edits, document isolation, text-cell skipping,
   result replacement, and continued execution after a structured failure.
3. **Representative product fixture.** Build one in-memory example covering
   exact arithmetic, assignment, assumptions or rewriting, polynomial algebra,
   and a deliberate failure. Assert canonical outputs and diagnostic codes;
   this fixture becomes the common input for later toolkit spikes.
4. **Consumer boundary review.** Confirm the new target depends toward the
   session/kernel only, no notebook code appears in the kernel, and no semantic
   logic is duplicated. Update build-target and contributor documentation with
   the new experimental target, clearly stating that no GUI or file format
   ships yet.

Baseline verification uses the existing Release symbolic and SDK suites.
Focused verification builds and runs notebook-core tests after each task.
Completion requires the focused tests, both affected existing suites,
`git diff --check`, local Markdown-link validation, and a final diff review.

### Deferred Decisions and Non-Goals

At delivery this slice did not choose Qt versus webview, a product name, or a
first distribution package. It did not add save/load, cached-output
compatibility, individual cell execution, cancellation, Markdown rendering,
rich mathematical display, autosave, gallery UI, export, plotting, or
packaging. Persistence was delivered by the following slice below; the
remaining items stay deferred.

## Delivered Persistence Slice

The notebook core now encodes and decodes version-1 UTF-8 JSON in memory and
loads or atomically saves local files. The persisted document contains ordered
input/text cells and optional generated results with canonical text,
diagnostics, and producer version.

Default limits are 8 MiB per file, 10,000 cells, 10,000 cached results, 256
bytes per cell identifier, 1 MiB per source/output/diagnostic text field, 8 MiB
aggregate source, and 128 diagnostics per result. Invalid references,
duplicate identifiers or results, unknown cell kinds, incompatible versions,
corrupt JSON, and exceeded limits fail with stable notebook errors. Loading
constructs a new document and never evaluates cells.

Autosave, recovery journals, compression, encryption, schema migrations, and
GUI file dialogs remain deferred.

## Delivered Cached-Result Clearing Slice

The notebook core can clear cached generated results in memory without
changing cells, source, format, version, or persisted files. Graphical stale
output presentation remains part of the planned UI contract.

## MVP Acceptance Evidence

Completion requires automated document round trips, version and corruption
tests, session isolation and clean `Run All` tests, diagnostic rendering tests,
verified gallery examples, packaging smoke tests, and a manual keyboard-only
workflow check. The kernel and CLI suites must remain green because the
notebook is a new consumer, not a replacement semantic path.
