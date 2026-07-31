# Aleph3 Web MVP Launch Plan

## Purpose

This plan defines the first web-accessible Aleph3 MVP. It is limited to
launching a usable symbolic notebook product surface backed by the current
kernel, session, packs, and notebook core. Broader Aleph3 roadmap work,
additional data packs, monetization, accounts, collaboration, plotting,
dynamic pack loading, hosted compute strategy, and broad CAS expansion are
explicitly future layers on top of this MVP.

The MVP should validate the product loop:

1. open a web app;
2. create or load a notebook;
3. add and edit text and input cells;
4. evaluate supported symbolic expressions;
5. see canonical output and diagnostics;
6. discover supported functions through completion and focused help;
7. run all cells from a clean session;
8. reset interactive state;
9. save and reload notebooks;
10. open verified example notebooks.

## Architectural Strategy

The web product is a consumer of existing Aleph3 semantics. It must not create
web-only parser rules, evaluator behavior, simplification rules, pack behavior,
or fallback semantics.

```text
React/Vite frontend
  -> reverse proxy
    -> C++ web API service
      -> anonymous client and session manager
      -> SQLite notebook store
      -> aleph3_notebook_core
      -> session::Session
      -> aleph3_kernel + registered packs
```

Ownership remains:

| Concern | Owner |
| --- | --- |
| expression meaning, exactness, evaluation, diagnostics, budgets | kernel |
| interactive definitions, reset, completion, help, request lifecycle | session |
| domain algorithms and symbols | registered packs |
| cells, document JSON, generated results, run-all lifecycle | notebook core |
| HTTP transport, request validation, anonymous limits, persistence access, completion/help serialization | web API |
| editing, rendering, stale-output presentation, completion/help presentation, user workflow | frontend |
| TLS, request body limits, edge rate limiting, static serving | reverse proxy |

## MVP Decisions

| Area | Decision |
| --- | --- |
| Deployment | local machine first; cloud VM soon after |
| Backend | C++ HTTP service linked to Aleph3 libraries |
| Frontend | React and Vite |
| Storage | SQLite |
| Authentication | no login for MVP |
| Users | anonymous multi-user |
| Abuse control | reverse proxy rate limiting plus backend limits |
| Session state | in-memory and TTL-based |
| Notebook persistence | SQLite records containing notebook JSON and metadata |
| Semantic core | existing kernel, session, notebook core, and registered packs only |
| Examples | verified against the current documented supported subset |

## Explicit Non-Goals

The MVP does not include:

- user accounts or paid plans;
- collaboration or sharing workflows;
- plotting or rich interactive visualizations;
- arbitrary code execution;
- arbitrary HTML/script execution in notebooks;
- dynamic pack loading, unload, or marketplace behavior;
- broad CAS claims or broad Mathematica compatibility;
- general solving, broad integration, or unrestricted calculus;
- durable cloud identity;
- production-grade hostile-code isolation beyond bounded request, session,
  storage, and evaluation controls.

## Anonymous User Strategy

Anonymous multi-user access is part of the MVP, so the service must treat all
traffic as untrusted even before login exists.

Use two high-entropy opaque identifiers:

- `anonymousClientId`, issued by the backend on first visit and stored in a
  cookie;
- `sessionId`, created per interactive session and tied to an anonymous
  client.

The anonymous client identifier is not an account and is not a durable identity
guarantee. It exists to provide soft ownership, quotas, and abuse controls
alongside IP-based reverse proxy limits. If a user clears cookies, access to
notebooks associated only with that anonymous client can be lost; this is an
accepted MVP limitation and must be documented in user-facing material.

Anonymous client rules:

- issue random high-entropy IDs server-side;
- associate notebooks with the anonymous client ID;
- reject cross-client notebook access;
- do not expose sequential notebook or session identifiers;
- apply quotas using both anonymous client ID and IP address where practical;
- avoid logging full notebook contents by default.

Session rules:

- sessions are in-memory process state;
- sessions are tied to one anonymous client;
- sessions expire after inactivity;
- explicit reset discards session-local definitions while preserving builtins
  and registered packs;
- live evaluator state is never persisted in SQLite;
- `Run All` reconstructs state from notebook source through a clean session.

Recommended configurable initial limits:

| Limit | Initial target |
| --- | --- |
| request body size | 1 MiB |
| notebook document size | 2-8 MiB |
| single cell source | 64-256 KiB |
| sessions per anonymous client | 3-5 |
| concurrent evaluations per anonymous client | 1 |
| idle session TTL | 30-60 minutes |
| notebooks per anonymous client | 20-50 |
| stored bytes per anonymous client | deployment-configured quota |

The reverse proxy must enforce request and rate limits, but the backend must
also enforce its own limits because proxy configuration can drift.

## Backend API Contract

The first API should be small and stable. All expected failures should return
structured JSON. Raw `Expr`, C++ pointers, or internal evaluator state must not
cross the HTTP boundary.

Initial endpoints:

```text
GET  /api/health
POST /api/clients

POST /api/sessions
GET  /api/sessions/{sessionId}
POST /api/sessions/{sessionId}/evaluate
POST /api/sessions/{sessionId}/reset
DELETE /api/sessions/{sessionId}
GET  /api/sessions/{sessionId}/complete?prefix={prefix}
GET  /api/sessions/{sessionId}/help?query={nameOrPrefix}

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

Representative successful evaluation response:

```json
{
  "status": "ok",
  "canonicalText": "5",
  "diagnostics": [],
  "elapsedMs": 4,
  "budget": {
    "exhausted": false
  }
}
```

Representative diagnostic response:

```json
{
  "status": "error",
  "canonicalText": null,
  "diagnostics": [
    {
      "code": "parse_error",
      "severity": "error",
      "message": "unexpected token"
    }
  ],
  "elapsedMs": 2,
  "budget": {
    "exhausted": false
  }
}
```

Backend behavior:

- validate JSON before touching session state;
- reject oversized input before evaluation;
- map invalid JSON, missing sessions, quota failures, oversized requests, and
  notebook ownership failures to stable HTTP status codes;
- serve completion and help from the shared session and registry metadata, not
  from a web-maintained list;
- include request IDs in logs and responses where useful;
- log endpoint, status, elapsed time, anonymous client ID hash, and failure
  class;
- do not log full expressions or notebook documents by default.

## SQLite Storage Plan

SQLite stores durable notebook records and lightweight anonymous usage
metadata. It does not store live evaluator state.

Suggested initial tables:

```text
anonymous_clients
notebooks
usage_counters
```

`notebooks` should include:

```text
id
anonymous_client_id
title
document_json
created_at
updated_at
last_opened_at
size_bytes
```

Storage rules:

- enable WAL mode;
- keep transactions short;
- do not hold database transactions during expression evaluation;
- validate notebook JSON and size limits before saving;
- preserve the existing notebook v1 document semantics where possible;
- periodically clean expired anonymous data if public deployment requires it.

## Frontend Scope

The web app opens directly into the notebook experience.

Required views and controls:

- notebook editor;
- minimal notebook list/open flow;
- examples browser;
- add input cell;
- add text cell;
- delete or clear cells if simple enough for MVP ergonomics;
- supported symbol completion while editing input cells;
- focused function help from completion details or a lightweight help panel;
- run cell;
- run all;
- reset session;
- save;
- open/copy example;
- clear generated results.

Cell behavior:

- input cells submit source to the backend;
- text cells are skipped by evaluation;
- editing input marks existing generated output stale;
- running one cell updates that cell output;
- `Run All` starts from clean session state and evaluates input cells in order;
- a failed input records diagnostics and does not stop later cells;
- canonical plain text is always renderable and copyable;
- diagnostics are displayed near the responsible cell.
- completion suggestions are discovery aids only; choosing a suggestion inserts
  supported syntax but does not change parser or evaluator behavior.

Deferred frontend behavior:

- rich math editor;
- AI completion or natural-language formula generation;
- plotting;
- collaboration;
- account management;
- export;
- plugin or pack marketplace UI;
- arbitrary embedded HTML or scripts.

## Supported Expression Scope

The UI and examples should advertise only currently documented and tested
behavior. Initial examples should be verified through automated tests or an
executable smoke check.

Candidate example expressions:

```text
1/2 + 1/3
a = 2
a + 3
Expand[(x + 1)^2]
Factor[x^2 - 1]
Refine[Sqrt[x^2], x >= 0]
Replace[f[x], f[a_] -> g[a]]
D[x^2 + 3*x, x]
Det[{{1, 2}, {3, 4}}]
```

Include one deliberate unsupported or invalid expression to demonstrate
diagnostics. Do not advertise planned capabilities until their contracts,
tests, and documentation ship.

## Delivery Phases

## Delivered API-Core Slice

The first implementation slice adds a transport-independent C++ web API core
and focused tests. It is intentionally below a real HTTP listener so request
validation, anonymous-client ownership, session isolation, reset behavior, and
diagnostic serialization can be tested before choosing or vendoring transport
infrastructure.

Delivered:

- `aleph3_web_api` library;
- `aleph3_web_api_server --health` smoke executable;
- `/api/health`;
- anonymous client creation;
- in-memory session creation, lookup, reset, deletion, and idle expiration;
- evaluate endpoint backed by `session::Session`;
- completion and focused help endpoints backed by `session::Session`;
- JSON success and error envelopes;
- ownership checks, session quota checks, source-size checks, request-body
  size checks, and malformed JSON diagnostics;
- focused `aleph3_web_api_tests` coverage.

Not delivered in this slice:

- HTTP socket listener;
- SQLite notebook persistence;
- notebook CRUD endpoints;
- `Run All` over persisted notebooks;
- frontend application;
- reverse proxy or deployment configuration.

The next slice should add SQLite notebook persistence only after preserving
the existing API-core tests.

### Phase 1: Contract And Skeleton

Deliver:

- API contract draft;
- C++ web service target;
- `/api/health`;
- JSON error envelope;
- local development run command;
- initial reverse proxy notes.

Exit criteria:

- service builds and runs locally;
- health endpoint works;
- no Aleph3 semantics are duplicated in web code.

### Phase 2: Anonymous Clients And Sessions

Deliver:

- anonymous client token issuance;
- session creation, lookup, reset, and deletion;
- session TTL cleanup;
- evaluate endpoint backed by `session::Session`;
- per-client and per-session limits;
- structured diagnostics.

Exit criteria:

- multiple anonymous clients can hold isolated sessions;
- assignments persist within one session;
- reset clears session-local definitions;
- expired sessions are cleaned.

### Phase 3: Completion And Help API Follow-Through

Delivered:

- session completion endpoint for editor prefixes;
- focused help endpoint for names, prefixes, packages, and supported REPL-like
  topics where they make sense in the web app;
- JSON schema for completion items and help entries;
- tests proving parity with CLI/session discovery for builtins, registered
  pack functions, and session-local user definitions.

Exit criteria met:

- the web API does not maintain a private completion catalog;
- completion/help results remain deterministic and finite;
- missing non-empty help queries fail with stable structured responses;
- prefix, package, and category help queries return stable structured lists;
- documentation states that completion covers the supported subset rather than
  broad Mathematica compatibility.

### Phase 4: SQLite Notebook Persistence

Deliver:

- SQLite schema and initialization;
- notebook create, list, load, save, and delete;
- ownership checks by anonymous client;
- storage quotas;
- document size validation.

Exit criteria:

- notebooks survive backend restart;
- invalid or oversized notebooks fail cleanly;
- cross-client notebook access is rejected;
- cookie loss limitation is documented.

### Phase 5: Notebook Run All Integration

Deliver:

- backend `run-all` using `aleph3_notebook_core`;
- generated result persistence;
- clear generated results;
- example notebook fixtures.

Exit criteria:

- `Run All` starts from clean state;
- definitions flow from earlier cells to later cells during one run;
- one failed cell does not stop later cells;
- examples are verified.

### Phase 6: React MVP UI

Deliver:

- notebook editor;
- text and input cells;
- completion and focused help inside the input-cell editing flow;
- run cell, run all, reset, save, and load;
- diagnostics and output rendering;
- examples gallery.

Exit criteria:

- full browser workflow works locally;
- desktop layout is usable;
- basic mobile layout does not break.

### Phase 7: Local And VM Deployment

Deliver:

- production frontend build;
- backend release build;
- reverse proxy configuration;
- rate limit configuration;
- request body limits;
- health check;
- deployment guide.

Exit criteria:

- local production-mode smoke test passes;
- VM deployment smoke test passes;
- reverse proxy limits and backend limits are both verified.

### Phase 8: Acceptance And Documentation

Deliver:

- web notebook manual page;
- API documentation;
- deployment notes;
- supported and unsupported examples;
- final test pass;
- final diff review.

Exit criteria:

- shipped examples match executable behavior;
- docs do not claim future features;
- affected C++ tests remain green;
- backend and frontend smoke tests pass.

## Verification Plan

Focused backend tests:

```text
health endpoint
anonymous client creation
session isolation
session reset
session expiry
evaluate success
evaluate diagnostic
oversized request rejection
unknown session rejection
quota rejection
```

Notebook persistence tests:

```text
create/save/load
list by anonymous client
reject cross-client notebook access
run all clean session
generated results persist
clear results
example notebooks evaluate
```

Frontend smoke tests:

```text
create notebook
add input cell
evaluate expression
show diagnostic
use completion to insert a supported function
open focused help for a supported function
save notebook
reload notebook
run all
reset session
open example
```

Affected existing Aleph3 verification:

```text
aleph3_symbolic_tests
aleph3_sdk_tests
aleph3_notebook_tests
```

Completion also requires `git diff --check`, local documentation-link checks
where available, and a final diff review for semantic duplication or stale
documentation.

## MVP Launch Definition Of Done

The MVP is launchable when:

- anonymous users can use isolated sessions;
- input editing includes deterministic supported-subset completion and focused
  help backed by shared session metadata;
- notebooks persist in SQLite;
- `Run All` reconstructs state from source;
- shipped examples are verified;
- reverse proxy and backend limits are active;
- local deployment works;
- VM deployment works;
- user-facing documentation states supported behavior and limitations;
- no web-only symbolic semantics exist.

## Principal Risks

Anonymous abuse is the largest launch risk. Mitigate it with high-entropy IDs,
client tokens, IP and client quotas, backend limits, reverse proxy limits,
session TTLs, and strict evaluation budgets.

Scope creep is the second major risk. Keep canonical text output first, defer
plotting, accounts, collaboration, dynamic packs, and broad CAS behavior.

Semantic drift is the third major risk. The backend should only call shared
session, notebook, kernel, and pack APIs. Missing symbolic behavior should be
planned later through the proper Aleph3 contracts rather than patched into the
web surface.
