# Sessions, CLI, And The Notebook

## The Session Contract

A session owns one reusable kernel evaluation context. It preserves definitions
and returns structured results. Current operations include evaluation,
simplification, full form, inspection, pack discovery, completion, and
diagnostics.

This is shared infrastructure for the CLI and future graphical products.

## CLI Workflow

`aleph3_cli repl` is the current local single-process kernel workbench. It
does not require the web BFF, the internal engine HTTP service, Traefik, or
Postgres:

```text
aleph3_cli repl
  -> session::Session
  -> kernel + registered packs
```

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
start fresh. On Windows and Unix-like interactive terminals, Tab completes
commands and symbols, arrow keys navigate history, and left/right arrows edit
the current line. `:complete` is the deterministic, pipe-friendly fallback.

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
a                         -> a

a = 2
f[x_] := x + a
f[3]                     -> 5
Clear[a]
f[3]                     -> a + 3
Unset[f]
f[3]                     -> a + 3
Clear[f]
f[3]                     -> f[3]
```

Delayed user functions evaluate their stored body against the current session
state when called. Clearing a referenced own value therefore affects later
calls, while `Unset[f]` does not remove `f`'s user function definition.
Provider-owned behavior keeps precedence over user definitions and remains
available after cleanup or `:reset`.

For stateful batch work, a script contains one expression per non-empty line:

```text
aleph3_cli script calculations.aleph3
aleph3_cli script --json calculations.aleph3
```

One session is shared across the file, failures do not stop later lines, and
the process exits with `2` if any expression fails. JSON mode emits one compact
object per submitted line with its line number, source, status, canonical
output, and unchanged session diagnostics. The `output` field is a JSON string
containing canonical text, so large exact integers and rationals are not
coerced into JSON numbers. Scripts are limited to 8 MiB and individual lines
to 1 MiB; comments and multiline expressions are unsupported.

## Headless Notebook Foundation

The current build includes the tested `aleph3_notebook_core` library, but not
a graphical notebook executable. The library models ordered input and text
cells with stable document-local identifiers. Its `Run All` operation starts a
fresh session, skips text cells, evaluates every input in order, and replaces
the previous generated results. Documents can also clear cached generated
results without changing cells or source.

Definitions and cleanup operations flow to later cells during a run using the
same session contract as the CLI. Repeating `Run All` starts clean, and one
failed input records its session diagnostics without preventing later inputs
from running. Generated results retain canonical plain text and current
diagnostic codes/messages. The core saves and loads bounded UTF-8 JSON v1
documents, including optional cached results marked with their producer
version. Cached `output` values are JSON strings containing canonical text,
including arbitrary-size exact integers and rationals. Loading preserves
cached results as data and never evaluates source; rerunning the notebook
replaces the cache from a fresh session.

Saves validate first, write beside the destination, and atomically replace the
old file using the supported platform API. A failed validation, write, or
replacement leaves the previous valid destination intact. Autosave, recovery
journals, and migrations are not implemented.

## Dormant Web/API Experiments

The current build includes tested web/API experiments, but they are not the
active product path and are not a public web notebook contract. They are
developer context for future work and must remain thin consumers of the shared
session and kernel.

The dormant browser-facing path is:

```text
browser -> BFF /api/* -> internal engine /internal/* -> session::Session
```

The C++ web API core exercises anonymous clients, sessions, notebook
persistence, `Run All`, examples, quotas, and ownership in tests. The internal
engine service is built as `aleph3_engine_service`; its smoke check is:

```text
aleph3_engine_service --health
```

The expected response is:

```json
{"ready":true,"service":"aleph3-engine","status":"ok"}
```

Detailed local commands for this dormant code live in
[Web Operations](../web_mvp_operations.md). A future web product should be
planned separately after the local notebook and private-kernel protocol are
stable.

## Graphical Notebook Status

No full graphical notebook application is included in the current build. The
delivered headless core and JSON format are product foundations rather than a
claim that notebook persistence, examples, completion/help UI, or `Run All`
have shipped in a graphical app. The near-term product path is the
Windows-first local graphical notebook; until that ships, `aleph3_cli repl`
remains the runnable local interactive fallback. In the planned public/private
split, that CLI stays private first-party tooling while public visibility
centers on the notebook and protocol/client layer.

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
