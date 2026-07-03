# Notebook MVP Design

## Status and Goal

This document is the proposed contract for the first Aleph3 notebook. It is a
design for planned behavior, not documentation of a shipped application.

The MVP succeeds when a user can launch a local application, create and edit a
document, evaluate supported symbolic input through one session, understand
results or failures, save the document, reopen it, and run bundled examples.

The product name, GUI toolkit, and physical file encoding remain open. Those
choices must preserve the behavioral contract below.

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
format; that decision follows a persistence prototype.

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

Cancellation, if supported by the selected toolkit and session boundary, must
cooperate with kernel budgets. Forcefully terminating arbitrary kernel work is
not an MVP promise until recovery and state integrity are specified.

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

## MVP Acceptance Evidence

Completion requires automated document round trips, version and corruption
tests, session isolation and clean `Run All` tests, diagnostic rendering tests,
verified gallery examples, packaging smoke tests, and a manual keyboard-only
workflow check. The kernel and CLI suites must remain green because the
notebook is a new consumer, not a replacement semantic path.

