# Sessions, CLI, And The Notebook

## The Session Contract

A session owns one reusable kernel evaluation context. It preserves definitions
and returns structured results. Current operations include evaluation,
simplification, full form, inspection, pack discovery, completion, and
diagnostics.

This is shared infrastructure for the CLI and future graphical products.

## CLI Workflow

```text
:help
:help Factor
:help core-algebra
:mode
:inspect f[x + 1]
:packs
:complete Pol
:reset
:quit
```

The symbolic REPL preserves assignments and definitions. One-shot commands
start fresh. On supported terminals Tab completes commands and symbols;
`:complete` is the deterministic, pipe-friendly fallback.

Use bare `:help` as the high-level discovery menu. It groups REPL commands,
builtins, special forms, discovered packs, and current user-defined names.
Focused help accepts a name, prefix, package, or REPL command:

```text
:help Factor
:help Clear
:help core-algebra
:help :reset
```

Focused help is backed by the shared session help metadata. Entries include
accepted forms, a concise description, short examples taken from the manual
where practical, exactness notes, unsupported boundaries, and the owning pack
or component when relevant. `:complete` uses the same session and registry
facts for deterministic name discovery.

Use `:reset` when an interactive symbolic session should start over without
leaving the REPL. It discards session-local assignments and user function
definitions, but it does not unload builtins or registered packs and does not
change the current `:mode`:

```text
a = 2
a                         -> 2
:reset
a                         -> a
Factor[x^2 - 1]           -> (x - 1) * (x + 1)
```

Use `Clear[symbol]` to remove a session-local own value and user function
definition. Use `Unset[symbol]` when only the own value should be removed:

```text
a = 10
Clear[a]
a

f[x_] := x + 1
Unset[f]
f[2]
```

For stateful batch work, a script contains one expression per non-empty line:

```text
aleph3_cli script calculations.aleph3
aleph3_cli script --json calculations.aleph3
```

One session is shared across the file, failures do not stop later lines, and
the process exits with `2` if any expression fails. JSON mode emits one compact
object per submitted line with its line number, source, status, canonical
output, and unchanged session diagnostics. Scripts are limited to 8 MiB and
individual lines to 1 MiB; comments and multiline expressions are unsupported.

## Headless Notebook Foundation

The current build includes an experimental `aleph3_notebook_core` library, but
not a graphical notebook executable. The library models ordered input and text
cells with stable document-local identifiers. Its `Run All` operation starts a
fresh session, skips text cells, evaluates every input in order, and replaces
the previous generated results. Documents can also clear cached generated
results without changing cells or source.

Definitions flow to later cells during a run. Repeating `Run All` starts clean,
and one failed input records its session diagnostics without preventing later
inputs from running. Generated results retain canonical plain text and current
diagnostic codes/messages. The core saves and loads bounded UTF-8 JSON v1
documents, including optional cached results marked with their producer
version. Loading never evaluates source.

Saves validate first, write beside the destination, and atomically replace the
old file using the supported platform API. A failed validation, write, or
replacement leaves the previous valid destination intact. Autosave, recovery
journals, and migrations are not implemented.

## Web API Foundation

The current build also includes an experimental `aleph3_web_api` library. It
is a transport-independent API core for the planned web notebook MVP. It is
now transitional contract evidence rather than the public browser backend. The
Phase 6a web slice adds an internal C++ engine HTTP service, an ASP.NET Core
BFF that owns public `/api/*` browser routes, a React/Vite evaluator surface,
and a Docker Compose graph through Traefik.

The API core still has a notebook store boundary; ordinary tests use an
in-memory store, and cloud-oriented builds can enable the Postgres store. The
companion `aleph3_web_api_server --health` executable remains a smoke check
for that API core.

The API core currently supports:

```text
GET  /api/health
POST /api/clients
POST /api/sessions
GET  /api/sessions/{sessionId}
POST /api/sessions/{sessionId}/evaluate
POST /api/sessions/{sessionId}/reset
GET  /api/sessions/{sessionId}/complete?prefix={prefix}
GET  /api/sessions/{sessionId}/help?query={nameOrPrefix}
DELETE /api/sessions/{sessionId}

POST /api/notebooks
GET  /api/notebooks
GET  /api/notebooks/{notebookId}
PUT  /api/notebooks/{notebookId}
DELETE /api/notebooks/{notebookId}
POST /api/notebooks/{notebookId}/run-all
POST /api/notebooks/{notebookId}/clear-results

GET  /api/examples
POST /api/examples/{exampleId}/copy
```

Legacy API-core session endpoints require the anonymous client identifier in
`X-Aleph3-Client`. Evaluation requests accept JSON with a string `source`
field and delegate directly to `session::Session` using the ordinary evaluate
operation:

```json
{"source":"1/2 + 1/3"}
```

A successful evaluation response contains canonical plain text and the current
session diagnostics array:

```json
{
  "status": "ok",
  "sessionId": "opaque-session-id",
  "result": {
    "status": "ok",
    "canonicalText": "5/6",
    "diagnostics": []
  }
}
```

Parse or evaluation failures are represented as successful API requests with
an error result and structured diagnostics. Invalid API input, missing clients,
unknown sessions, ownership failures, quota failures, oversized requests, and
unknown routes use a JSON error envelope instead.

Completion and focused help endpoints delegate to the same session discovery
operations used by the CLI. Completion returns deterministic supported-subset
matches for builtins, registered pack functions, and session-local definitions:

```json
{
  "status": "ok",
  "sessionId": "opaque-session-id",
  "prefix": "Fac",
  "completions": [
    {
      "name": "Factor",
      "category": "pack",
      "owningPackage": "core-algebra",
      "documentation": "Factor a supported exact polynomial expression."
    }
  ]
}
```

Focused help accepts a name, prefix, package, or category query where the
shared session catalog supports it. Unknown non-empty help queries return a
stable JSON error envelope instead of an empty success response. These
endpoints are discovery aids only; inserting a completion never changes parser
or evaluator behavior.

Sessions are in-memory only in this slice. They are isolated by anonymous
client, expire after the configured idle TTL, and enforce a per-client
active-session limit. Notebook documents are persisted through the web
notebook store boundary. Production web persistence is Postgres-backed when the
backend is built with Postgres support and configured with
`ALEPH3_DATABASE_URL`. In the Web MVP architecture, BFF-owned Postgres
persistence replaces this C++ product-store path in a later slice.

Notebook endpoints persist versioned notebook JSON and validate it through the
same headless notebook core used by local file persistence. Create requests may
omit a document to create an empty notebook, or provide either a JSON
`document` object or a string `documentJson` field:

```json
{
  "title": "Scratch",
  "document": {
    "format": "aleph3-notebook",
    "version": 1,
    "cells": [
      {"id": "cell-1", "kind": "input", "source": "1/2 + 1/3"}
    ]
  }
}
```

Notebook records are owned by the anonymous client in `X-Aleph3-Client`.
Cross-client load, save, or delete attempts fail with a structured ownership
error. Because the anonymous client identifier is cookie-backed in the planned
web deployment, clearing cookies can lose access to notebooks tied only to that
identifier. Invalid notebook JSON, unsupported document versions, oversized
documents, title limits, per-client notebook count limits, and stored-byte
quota failures are rejected before saving. Live evaluator state is not
persisted in Postgres.

`POST /api/notebooks/{notebookId}/run-all` loads the persisted notebook
document, delegates to the headless notebook runner, and saves the resulting
generated-result cache back through the notebook store. It starts from a clean
session, skips text cells, evaluates input cells in order, lets definitions
flow to later cells during that one run, and records diagnostics without
stopping later cells:

```json
{
  "status": "ok",
  "notebook": {
    "id": "opaque-notebook-id",
    "title": "Scratch",
    "document": {
      "format": "aleph3-notebook",
      "version": 1,
      "cells": [
        {"id": "define", "kind": "input", "source": "a = 2"},
        {"id": "use", "kind": "input", "source": "a + 3"}
      ],
      "results": [
        {
          "source_cell_id": "define",
          "ok": true,
          "output": "2",
          "diagnostics": [],
          "producer_version": "unknown"
        },
        {
          "source_cell_id": "use",
          "ok": true,
          "output": "5",
          "diagnostics": [],
          "producer_version": "unknown"
        }
      ]
    }
  }
}
```

`POST /api/notebooks/{notebookId}/clear-results` removes persisted generated
results while preserving cells and source. It does not reset or mutate any
live interactive session.

The example endpoints expose verified read-only notebook templates and copy a
template into a notebook owned by the requesting anonymous client. Copying an
example does not evaluate it; use `run-all` on the copied notebook to generate
fresh cached results. The first example catalog intentionally advertises only
the supported subset covered by existing tests: exact arithmetic, assignments,
algebra, assumptions, rewriting, focused differentiation, exact matrices, and
one deliberate parse diagnostic.

## Phase 6a Web Evaluation Loop

The current browser-facing web slice is deliberately narrow:

```text
browser -> BFF /api/* -> internal engine /internal/* -> session::Session
```

The internal engine service is built as `aleph3_engine_service`. Its smoke
check is:

```text
aleph3_engine_service --health
```

The expected response is:

```json
{"ready":true,"service":"aleph3-engine","status":"ok"}
```

When run as a listener, the engine exposes only internal computation routes
such as:

```text
GET  /internal/health
POST /internal/sessions
POST /internal/sessions/{sessionId}/evaluate
POST /internal/sessions/{sessionId}/reset
```

The ASP.NET Core BFF exposes the first public browser API:

```text
GET  /api/health
POST /api/sessions
POST /api/sessions/{sessionId}/evaluate
```

The BFF validates browser JSON, forwards evaluation to the engine, and maps
engine failures into public JSON error envelopes. It does not parse,
evaluate, simplify, or maintain symbolic help catalogs. The React/Vite
frontend creates a session, sends input source to the BFF, and renders
canonical plain text plus diagnostics. Running `1/2 + 1/3` through the browser
should display `5/6`.

The production-like Compose graph routes `/` to the frontend and `/api/*` to
the BFF through Traefik. The engine and Postgres services are internal-only in
that profile. The development Compose override may publish frontend, BFF,
engine, and Postgres ports for debugging.

## Graphical Notebook Status

No full graphical notebook application is included in the current build. The
Phase 6a browser surface is a single evaluator loop, while the delivered
headless core and JSON format remain product foundations rather than a claim
that notebook persistence, examples, completion/help UI, or `Run All` have
shipped in the browser. Until the full web notebook loop ships, `aleph3_cli
repl` remains the runnable local interactive fallback.

The planned application remains a thin consumer: the GUI owns cells,
presentation, and file interaction; the session owns interactive state; the
kernel and packs own semantics. The first verified gallery should begin with
current behavior such as:

```text
1/2 + 1/3
Refine[Sqrt[x^2], x >= 0]
Replace[f[x], f[a_] -> g[a]]
Factor[(1/2)*x^2 + x + 1/2]
PolynomialQuotient[x^2*y + x*y^2 + y, x*y, {x, y}]
D[x^2 + 3*x, x]
Det[{{1, 2}, {3, 4}}]
```

Plotting, code generation, export, rich Markdown, and other roadmap
capabilities must not appear as current examples until their contracts and
tests ship. The [Notebook MVP Design](../notebook_mvp_design.md)
owns the planned document and UI contract; the
[Unified Plan](../aleph3_unified_plan.md) owns sequencing.
