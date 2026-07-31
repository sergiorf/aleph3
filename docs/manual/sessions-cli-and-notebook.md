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
is a transport-independent API core for the planned web notebook MVP, not yet a
network listener, notebook store, or deployed service. The companion
`aleph3_web_api_server --health` executable is only a smoke check for the API
core.

The API core currently supports:

```text
GET  /api/health
POST /api/clients
POST /api/sessions
GET  /api/sessions/{sessionId}
POST /api/sessions/{sessionId}/evaluate
POST /api/sessions/{sessionId}/reset
DELETE /api/sessions/{sessionId}
```

Session endpoints require the anonymous client identifier in
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

Anonymous clients and sessions are in-memory only in this slice. Sessions are
isolated by anonymous client, expire after the configured idle TTL, and enforce
a per-client active-session limit. No notebooks are persisted through this API
yet, and there is no HTTP server, SQLite store, browser frontend, reverse
proxy configuration, or production deployment in the current build.

## Graphical Notebook Status

No graphical notebook application is included in the current build. The
delivered headless core and JSON format are product foundations, not a claim
that a desktop UI has shipped. Until it does, `aleph3_cli repl` is the runnable
local interactive surface.

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
