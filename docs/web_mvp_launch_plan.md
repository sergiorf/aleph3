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
  -> Traefik reverse proxy
    -> ASP.NET Core BFF
      -> Postgres product store
      -> internal Aleph3 C++ engine service
        -> session::Session
        -> aleph3_kernel + registered packs
```

Ownership remains:

| Concern | Owner |
| --- | --- |
| expression meaning, exactness, evaluation, diagnostics, budgets | kernel |
| interactive definitions, reset, completion, help, request lifecycle | session |
| domain algorithms and symbols | registered packs |
| notebook document semantics, v1 JSON validation, generated-result shape, clean run-all rules | notebook core |
| public browser API, anonymous identity, notebook ownership, persistence, examples, generated-result persistence, product quotas, public error envelopes | ASP.NET Core BFF |
| internal session HTTP transport, evaluation request validation, completion/help serialization, computation limits | C++ engine service |
| editing, rendering, stale-output presentation, completion/help presentation, user workflow | frontend |
| TLS, request body limits, edge rate limiting, public routing | Traefik reverse proxy |

The BFF is the public product backend. The C++ service is internal computation
infrastructure and must not be routed directly from the public reverse proxy.
The current transport-independent `aleph3_web_api` implementation is useful
repository evidence and may be adapted, narrowed, or retired as this boundary
is introduced; it must not remain a competing public backend owner.

## MVP Decisions

| Area | Decision |
| --- | --- |
| Deployment | single cloud VM MVP, with local development support |
| Orchestration | Docker Compose, no Kubernetes |
| Reverse proxy | Traefik |
| Public backend | ASP.NET Core BFF |
| Engine backend | internal C++ HTTP service linked to Aleph3 libraries |
| Frontend | React and Vite |
| Storage | Postgres |
| Authentication | no login for MVP |
| Users | anonymous multi-user |
| Abuse control | Traefik rate limiting plus BFF and engine limits |
| Session state | in-memory and TTL-based in the engine service |
| Notebook persistence | BFF-owned Postgres records containing notebook JSON, metadata, and generated results |
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
alongside IP-based Traefik limits. If a user clears cookies, access to
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
- live evaluator state is never persisted in Postgres;
- `Run All` reconstructs state from notebook source through a clean engine
  session.

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

Traefik must enforce edge request and rate limits, but the BFF and engine must
also enforce their own limits because proxy configuration can drift and
internal callers are still untrusted from the engine's perspective.

## Backend API Contract

The first public API should be small and stable. It is exposed only by the
BFF. All expected failures should return structured JSON. Raw `Expr`, C++
pointers, internal evaluator state, engine implementation errors, and internal
service addresses must not cross the public HTTP boundary.

Initial public BFF endpoints:

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

BFF behavior:

- validate JSON before touching session state;
- reject oversized source and notebook input before forwarding work to the
  engine;
- map invalid JSON, missing sessions, quota failures, oversized requests,
  engine failures, and notebook ownership failures to stable public HTTP
  status codes;
- own anonymous identity, notebook ownership, notebook persistence, examples,
  generated-result persistence, and product-facing quotas;
- serve completion and help by delegating to the engine's shared session and
  registry metadata, not from a BFF-maintained catalog;
- include request IDs in logs and responses where useful;
- log endpoint, status, elapsed time, anonymous client ID hash, and failure
  class;
- do not log full expressions or notebook documents by default.

Initial internal C++ engine endpoints:

```text
GET  /internal/health

POST /internal/sessions
GET  /internal/sessions/{sessionId}
POST /internal/sessions/{sessionId}/evaluate
POST /internal/sessions/{sessionId}/reset
DELETE /internal/sessions/{sessionId}
GET  /internal/sessions/{sessionId}/complete?prefix={prefix}
GET  /internal/sessions/{sessionId}/help?query={nameOrPrefix}
```

The internal engine API may evolve faster than the public BFF API, but it must
remain structured and testable. It owns symbolic sessions, parsing,
evaluation, completion, help, canonical output, diagnostics, and computation
budgets. It does not own browser cookies, product notebook ownership, public
notebook CRUD, examples, billing-ready identity, or Postgres product tables.

## Postgres Storage Plan

Postgres is owned by the BFF. It stores durable notebook records, generated
result caches, examples metadata where needed, anonymous-client metadata, and
lightweight usage counters for the cloud VM deployment. It does not store live
evaluator state.

Suggested initial tables:

```text
anonymous_clients
notebooks
notebook_results
examples
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

- keep transactions short;
- do not hold database transactions during expression evaluation;
- validate notebook JSON and size limits before saving;
- preserve the existing notebook v1 document semantics where possible;
- periodically clean expired anonymous data if public deployment requires it.
- configure the BFF connection string through environment or deployment
  secrets;
- keep connection handling explicit and add pooling according to ASP.NET Core
  and Postgres provider guidance;
- do not let the C++ engine service and BFF write the same product tables.

The existing C++ Postgres-backed notebook store is transitional in the BFF
architecture. It may remain useful for tests or migration, but the public web
MVP should converge on BFF-owned persistence rather than shared table
ownership.

## Docker Compose And Traefik Plan

The MVP uses Docker Compose on one VM. It should stay intentionally simple:
one reverse proxy, one frontend, one BFF, one engine service, and one
Postgres service. Kubernetes, service meshes, distributed job queues, and
multi-node scheduling are non-goals.

Initial services:

```text
traefik
frontend
bff
engine
postgres
```

Networks:

```text
public
  traefik
  frontend
  bff

internal
  bff
  engine
  postgres
```

Only Traefik publishes host ports in the VM deployment:

```text
80:80
443:443
```

Public routes:

```text
/      -> frontend
/api/* -> bff
```

Internal routes:

```text
bff -> http://engine:8080
bff -> postgres:5432
```

The engine and Postgres containers must not have public Traefik routers. Local
development may use a Compose override to expose Vite, the BFF, the engine, or
Postgres directly for debugging, but the production-like Compose file should
publish only Traefik.

Required volumes:

```text
postgres_data
traefik_letsencrypt
```

Required health checks:

- Traefik process health;
- BFF `/api/health`;
- engine `/internal/health`;
- Postgres readiness.

Compose `depends_on` may express startup order, but the BFF and frontend must
still tolerate dependency startup delays with retries or clear startup
failures. Compose ordering is not a replacement for readiness handling.

Traefik responsibilities:

- TLS termination on the VM;
- route `/` to the static frontend service;
- route `/api/*` to the BFF service;
- enforce edge request-size and rate-limit middleware;
- keep any dashboard disabled publicly or protected;
- avoid routing public traffic to the engine.

Development profile:

```text
docker compose up postgres engine bff
cd web && npm run dev
```

or:

```text
docker compose -f docker-compose.yml -f docker-compose.dev.yml up
```

The development override may publish frontend, BFF, engine, and Postgres ports
only for local debugging. The VM profile should keep engine and Postgres
internal.

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

After the BFF decision, this delivered work is best treated as repository
evidence and transitional infrastructure. The session, evaluation, completion,
help, notebook validation, and run-all behavior remain valuable. Public
browser API ownership, product persistence, anonymous identity, and examples
move to the BFF plan instead of staying in a public C++ backend.

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
- `Run All` over persisted notebooks;
- frontend application;
- Traefik, BFF, or deployment configuration.

Postgres-oriented notebook persistence and notebook CRUD endpoints were
delivered in the following slice. Backend `Run All`, generated-result
persistence, clear-results, and verified example notebook fixtures were
delivered after that.

## BFF Migration Plan

The BFF is implemented at the start of Phase 6, not after the React UI is
complete. Phase 6a creates the BFF process, public `/api/*` boundary, internal
engine call path, and Compose service graph before notebook persistence or the
full editor is built. From that point forward, browser-facing work targets the
BFF API first.

The existing C++ `aleph3_web_api` code should be handled as a migration source
with three possible outcomes for each responsibility:

- move product responsibility to the BFF;
- preserve computation/session behavior behind an internal engine endpoint;
- keep C++ tests as contract evidence until equivalent BFF or end-to-end tests
  exist, then narrow or retire the old API-core surface.

Responsibility migration:

| Responsibility | Current location | Target owner | Migration phase | Notes |
| --- | --- | --- | --- | --- |
| public `/api/*` route shape | `aleph3_web_api` test harness | BFF | 6a | The browser never binds to direct C++ routes. |
| anonymous client cookie/bootstrap | `aleph3_web_api` | BFF | 6b | BFF issues and validates browser identity. |
| notebook ownership checks | `aleph3_web_api` | BFF | 6b | Ownership is product policy, not engine policy. |
| notebook CRUD | `aleph3_web_api` + C++ store | BFF | 6b | BFF writes Postgres product tables. |
| notebook JSON validation | `aleph3_notebook_core` | BFF using shared contract or internal validation bridge | 6b | Prefer reusing the v1 document contract; do not invent a second format. |
| generated-result persistence | `aleph3_web_api` + C++ store | BFF | 6b-6c | Results remain display cache. |
| run cell orchestration | `aleph3_web_api` session endpoint | BFF + engine session endpoint | 6c | BFF maps public request to internal engine evaluation. |
| `Run All` orchestration | `aleph3_web_api` + `aleph3_notebook_core` | BFF orchestration over engine sessions | 6c | BFF loads source, starts clean engine session, persists generated results. |
| clear results | `aleph3_web_api` + C++ store | BFF | 6b | No engine call required. |
| examples catalog and copy | `aleph3_web_api` | BFF | 6d | Examples must still be verified against the engine. |
| symbolic sessions | `aleph3_web_api` | C++ engine service | 6a-6c | Engine owns in-memory session state and TTL. |
| evaluate/reset | `session::Session` through `aleph3_web_api` | C++ engine service | 6a | BFF delegates; it does not evaluate. |
| completion/help | `session::Session` through `aleph3_web_api` | C++ engine service, exposed through BFF | 6d | BFF forwards and shapes public envelopes only. |
| Postgres product tables | optional C++ Postgres store | BFF | 6b | Do not share write ownership. |

Phase 6 should avoid a long-lived compatibility layer where React can call
either backend. During development, direct engine access may be exposed through
a local Compose override for debugging, but production-like runs and browser
tests must use React -> Traefik -> BFF -> engine.

The old C++ public API endpoints are not removed in the same slice merely to
make the diff clean. They should be narrowed only after replacement coverage
exists:

1. Phase 6a proves BFF -> engine evaluation and keeps existing
   `aleph3_web_api_tests` as engine/session regression coverage.
2. Phase 6b adds BFF notebook CRUD and ownership tests, then marks C++
   notebook-store tests as legacy migration coverage.
3. Phase 6c adds BFF run-cell and run-all tests, then stops treating C++
   persisted `run-all` as the launch path.
4. Phase 6d adds BFF completion/help/examples tests and browser smoke tests,
   then narrows remaining C++ web API documentation to internal or legacy
   status.

Completion of Phase 6 requires one authoritative public API owner: the BFF.
Any remaining C++ web API target must be documented as internal, transitional,
or test-only.

### Phase 1: Contract And Skeleton

Deliver:

- API contract draft;
- C++ web service target;
- `/api/health`;
- JSON error envelope;
- local development run command;
- initial deployment boundary notes.

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

### Phase 4: Postgres Notebook Persistence

Delivered:

- Postgres store contract, schema initialization, and optional libpq-backed
  implementation;
- notebook create, list, load, save, and delete;
- ownership checks by anonymous client;
- storage quotas;
- document size validation.

Exit criteria met in the API-core test harness:

- invalid or oversized notebooks fail cleanly;
- cross-client notebook access is rejected;
- cookie loss limitation is documented.

Exit criteria that applied to the earlier C++ Postgres path:

- notebooks survive backend process restart against a real Postgres database;
- production build links against libpq with `ALEPH3_ENABLE_POSTGRES=ON`;
- `ALEPH3_DATABASE_URL` is documented and exercised in a deployment smoke
  check.

These are no longer launch criteria for the public web MVP after the BFF
decision. The BFF-owned Postgres path carries the future deployment criteria.

Not delivered in this slice:

- `Run All` over persisted notebooks;
- generated result persistence updates through `run-all`;
- clear generated results endpoint;
- example notebook fixtures;
- HTTP socket listener or frontend application.

### Phase 5: Notebook Run All Integration

Delivered:

- backend `run-all` using `aleph3_notebook_core`;
- generated result persistence;
- clear generated results;
- example notebook fixtures.

Exit criteria met in the API-core test harness:

- `Run All` starts from clean state;
- definitions flow from earlier cells to later cells during one run;
- one failed cell does not stop later cells;
- examples are verified.

Not delivered in this slice:

- HTTP socket listener;
- browser frontend;
- Traefik, BFF, or deployment configuration.

### Phase 6a: BFF Boundary And First Evaluation Loop

Deliver:

- ASP.NET Core BFF skeleton;
- minimal internal C++ HTTP listener over shared session behavior;
- BFF public API envelope and client-facing error mapping;
- migration of public evaluate traffic from the old C++ API-core route shape
  to the BFF route shape;
- React/Vite skeleton opened directly into a minimal notebook-like surface;
- Traefik-ready routing labels or local routing notes;
- Docker Compose services for `traefik`, `frontend`, `bff`, `engine`, and
  `postgres`;
- internal engine health endpoint;
- public BFF health endpoint;
- public browser-facing evaluate endpoint;
- one browser input flow that evaluates through BFF -> engine.

Exit criteria:

- browser traffic reaches only the BFF through `/api/*`;
- the engine service is reachable only on the internal Docker network in the
  production-like Compose profile;
- entering `1/2 + 1/3` in the browser shows `5/6`;
- BFF and engine health checks pass;
- no symbolic parser, evaluator, simplifier, or completion catalog is
  duplicated in the BFF or frontend;
- existing C++ API-core evaluate tests remain as engine/session regression
  evidence or are replaced by equivalent internal engine tests;
- Docker Compose can start the local service graph.

### Phase 6b: BFF-Owned Notebook Persistence

Deliver:

- BFF Postgres schema and migrations for anonymous clients, notebooks,
  generated results or result cache fields, examples metadata where needed,
  and usage counters;
- anonymous client cookie/bootstrap flow owned by the BFF;
- migration of public anonymous-client, ownership, notebook CRUD, and clear
  results behavior from `aleph3_web_api` to BFF handlers;
- notebook create, list, load, save, and delete;
- ownership checks by anonymous client;
- notebook size, title, count, and stored-byte limits;
- React notebook list/open flow;
- notebook editor with input and text cells;
- save and reload workflow;
- clear generated results workflow.

Exit criteria:

- notebook records survive BFF and engine restart against Postgres;
- cross-client notebook access is rejected by the BFF;
- invalid or oversized notebook documents fail with stable public envelopes;
- C++ engine does not write BFF-owned product tables;
- C++ notebook persistence tests are either replaced by BFF persistence tests
  or explicitly retained as legacy migration coverage;
- clearing results preserves cells and source.

### Phase 6c: Run Cell, Run All, Reset, And Results

Deliver:

- run-cell flow through BFF -> engine session evaluation;
- per-anonymous-client session creation, lookup, reset, deletion, TTL, and
  concurrent-evaluation limits;
- migration of public run-cell, reset, session lifecycle, generated-result
  persistence, and run-all behavior from `aleph3_web_api` to BFF handlers plus
  internal engine calls;
- generated-result persistence after run cell and run all;
- `Run All` from persisted source through a clean engine session;
- failed-cell diagnostics without stopping later cells;
- stale-output marking when input source changes after generated output exists;
- canonical plain-text output rendering and copying.

Exit criteria:

- assignments persist within one interactive session;
- reset clears session-local definitions while preserving builtins and packs;
- `Run All` starts clean and definitions flow from earlier cells to later
  cells during that run;
- one failed input records diagnostics and later input cells still run;
- live evaluator state is never persisted in Postgres;
- results are persisted as display cache, not semantic authority.
- C++ persisted `run-all` remains only internal, transitional, or test-only
  after equivalent BFF run-all coverage exists.

### Phase 6d: Completion, Help, Examples, And UI Completion

Deliver:

- completion endpoint in the BFF delegating to the engine;
- focused help endpoint in the BFF delegating to the engine;
- migration of public completion, help, examples list, and examples copy
  behavior from `aleph3_web_api` to BFF handlers;
- completion and focused help inside the input-cell editing flow;
- examples list and copy flow owned by the BFF;
- verified example notebook fixtures using only the documented supported
  subset;
- diagnostics and output rendering polish;
- responsive desktop and basic mobile layout;
- frontend smoke tests for the complete product loop.

Exit criteria:

- completion/help results come from shared session and registry metadata, not
  from a BFF or frontend-maintained catalog;
- examples are verified against the current engine;
- full browser workflow works locally;
- desktop layout is usable;
- basic mobile layout does not break;
- user-facing documentation states anonymous-cookie limitations, supported
  behavior, and unsupported boundaries.
- remaining C++ web API documentation is narrowed to internal, transitional,
  or test-only status.

### Phase 7: Local And VM Deployment

Deliver:

- GitHub Actions build, test, and formatting checks;
- production frontend build;
- BFF release build and container image;
- engine release build and container image;
- Postgres service provisioning through Compose or managed connection
  configuration;
- Traefik configuration through Compose labels, middleware, and persisted TLS
  storage;
- Docker Compose production profile;
- Docker Compose development override;
- rate limit configuration;
- request body limits;
- BFF and engine health checks;
- structured BFF, engine, and Traefik logging with correlated request IDs
  where practical;
- minimal operational metrics;
- deployment guide.

Exit criteria:

- repository CI builds and runs tests on supported hosted platforms;
- changed C++ source/header files are format-checked in CI;
- local production-mode smoke test passes;
- VM deployment smoke test passes;
- Postgres connectivity and schema initialization are verified;
- Traefik limits, BFF limits, and engine limits are all verified;
- only Traefik exposes host ports in the VM profile;
- public routes send `/` to the frontend and `/api/*` to the BFF;
- no public route reaches the engine;
- logs include request id, endpoint, status, elapsed time, anonymous client id
  hash where applicable, and failure class without full notebook/source
  contents by default.

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

Focused BFF and engine tests:

```text
BFF health endpoint
engine health endpoint
anonymous client creation
session isolation through BFF-owned client identity
session reset
session expiry
evaluate success through BFF -> engine
evaluate diagnostic through BFF -> engine
oversized request rejection at BFF and engine boundaries
unknown session rejection
quota rejection
engine unavailable error mapping
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

Compose and deployment smoke tests:

```text
Traefik routes / to the frontend
Traefik routes /api/* to the BFF
engine has no public Traefik route
BFF reaches engine on the internal network
BFF reaches Postgres on the internal network
only Traefik publishes host ports in the VM profile
health checks pass for BFF, engine, and Postgres
```

Affected existing Aleph3 verification:

```text
aleph3_symbolic_tests
aleph3_sdk_tests
aleph3_notebook_tests
```

Repository automation:

```text
GitHub Actions CMake build and CTest on Ubuntu and Windows
GitHub Actions clang-format check for changed C++ source/header files
```

Completion also requires `git diff --check`, local documentation-link checks
where available, and a final diff review for semantic duplication or stale
documentation.

## MVP Launch Definition Of Done

The MVP is launchable when:

- anonymous users can use isolated sessions;
- input editing includes deterministic supported-subset completion and focused
  help backed by shared session metadata;
- notebooks persist in BFF-owned Postgres tables;
- `Run All` reconstructs state from source;
- shipped examples are verified;
- Traefik, BFF, and engine limits are active;
- the engine service is internal-only and not publicly routed;
- Docker Compose starts the production-like VM profile;
- local deployment works;
- VM deployment works;
- user-facing documentation states supported behavior and limitations;
- no web-only symbolic semantics exist.

## Principal Risks

Anonymous abuse is the largest launch risk. Mitigate it with high-entropy IDs,
client tokens, IP and client quotas, BFF limits, Traefik limits, engine
limits, session TTLs, and strict evaluation budgets.

Scope creep is the second major risk. Keep canonical text output first, defer
plotting, accounts, collaboration, dynamic packs, and broad CAS behavior.

Semantic drift is the third major risk. The backend should only call shared
session, notebook, kernel, and pack APIs. Missing symbolic behavior should be
planned later through the proper Aleph3 contracts rather than patched into the
web surface.

Backend ownership drift is the fourth major risk. The BFF and C++ engine must
not both become product backends. Keep Postgres ownership, public browser API
shape, notebook ownership, examples, generated-result persistence, and future
account policy in the BFF. Keep expression parsing, evaluation, completion,
help, diagnostics, and budgets in the engine.
