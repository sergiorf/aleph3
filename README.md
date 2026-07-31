![CI Status](https://img.shields.io/github/actions/workflow/status/sergiorf/aleph3/build.yml?label=CI)
![License](https://img.shields.io/github/license/sergiorf/aleph3)

<p align="center">
  <img src="assets/logo.png" alt="Aleph3 Logo" width="200"/>
</p>

# Aleph3
Aleph3 is evolving into a lightweight web-accessible symbolic notebook and
local computation environment written in modern C++. The first MVP is a web
notebook surface backed by the same C++ kernel, session layer, notebook core,
and registered math packs used by the CLI and SDK.

The repository currently contains the foundation for that MVP:

Use it to:

- build the experimental web API core for anonymous clients and isolated
  symbolic sessions
- evaluate supported symbolic expressions through shared session semantics
- discover supported functions through session-backed completion and focused
  help metadata
- run the CLI as the current interactive local fallback
- validate and execute application formulas through a small SDK
- work with exact rationals, assumptions, rewrite rules, and polynomial algebra
- keep web, CLI, SDK, sessions, notebook documents, and math packs on one
  semantic path

The web MVP is being assembled in slices. The current tree includes the
transport-independent legacy web API core, an internal C++ engine HTTP service,
an ASP.NET Core BFF skeleton, a React/Vite evaluator surface, and Docker
Compose routing through Traefik. Notebook persistence, examples in the browser,
completion/help UI, and `Run All` through the BFF remain later MVP slices. The
CLI remains the easiest fully local interactive fallback.

## Build Notifications

GitHub Actions runs the `CI` workflow for pushes and pull requests targeting
`main`. The workflow builds and runs the CTest suite on Ubuntu and Windows, and
checks changed C++ source/header formatting with `clang-format` on Ubuntu.

Aleph3 is deliberately focused rather than a Mathematica, SageMath, or
general-purpose CAS replacement. Unsupported algebraic forms fail explicitly
instead of silently guessing.

## Syntax

Aleph3 currently accepts a Wolfram-like expression syntax for its symbolic and
CLI-facing workflows.

Examples:

- `If[x >= 1, "ok", False]`
- `Refine[Sqrt[x^2], x >= 0]`
- `Replace[f[f[x], x], x -> y, {1, 2}]`
- `PolynomialQuotient[x^2*y + x*y^2 + y, x*y, {x, y}]`

That syntax is a current frontend, not the whole product identity.
The long-term kernel design keeps syntax separate from semantics so Aleph3 can
support compatibility syntax, a more Aleph3-native syntax, or both over time.

## Build And Launch The Web API Core

You need CMake 3.20+ and a C++20 compiler.

1. Clone and configure:

   ```bash
   git clone https://github.com/sergiorf/aleph3.git
   cd aleph3
   cmake -S . -B build
   ```

2. Build the web API core and smoke executable:

   ```bash
   cmake --build build --config Release --target aleph3_web_api_server
   ```

3. Launch the current health-check entrypoint:

   ```bash
   ./build/bin/aleph3_web_api_server --health
   ```

   On Visual Studio and other multi-configuration generators:

   ```powershell
   .\build\bin\Release\aleph3_web_api_server.exe --health
   ```

   Expected output:

   ```json
   {"ready":true,"service":"aleph3-web-api","status":"ok"}
   ```

This proves the current API-core executable is built and runnable. It does not
start the public web MVP backend. The implemented API core is exercised
through tests and remains transitional contract evidence while browser traffic
migrates to the BFF:

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
```

Session endpoints use anonymous client ownership and delegate evaluation,
completion, and focused help to `session::Session`; web code does not add
symbolic parser, evaluator, discovery, or pack semantics.

## Run The Phase 6a Web MVP Slice

The first browser loop uses this path:

```text
React/Vite frontend -> ASP.NET Core BFF /api/* -> internal C++ engine /internal/* -> session::Session
```

### Internal C++ Engine

Build and smoke-test the internal engine service:

```bash
cmake --build build --config Release --target aleph3_engine_service
./build/bin/aleph3_engine_service --health
```

On Visual Studio and other multi-configuration generators:

```powershell
cmake --build build --config Release --target aleph3_engine_service
.\build\bin\Release\aleph3_engine_service.exe --health
```

Expected output:

```json
{"ready":true,"service":"aleph3-engine","status":"ok"}
```

Run the focused engine tests:

```bash
cmake --build build --config Release --target aleph3_engine_api_tests
ctest --test-dir build -C Release -R aleph3_engine_api_tests --output-on-failure
```

Start the internal engine listener for local BFF development:

```bash
ALEPH3_ENGINE_PORT=8080 ./build/bin/aleph3_engine_service
```

On PowerShell:

```powershell
$env:ALEPH3_ENGINE_PORT = "8080"
.\build\bin\Release\aleph3_engine_service.exe
```

### ASP.NET Core BFF

The BFF exposes the public browser API and forwards evaluation to the internal
engine. It requires the .NET 8 SDK for local development:

```bash
cd web/bff
dotnet run
```

By default it calls `http://localhost:8080`. Override the engine URL when
needed:

```bash
ALEPH3_ENGINE_BASE_URL=http://localhost:8080 dotnet run --project web/bff/Aleph3.Bff.csproj
```

Smoke-test the BFF:

```bash
curl http://localhost:5000/api/health
curl -X POST http://localhost:5000/api/sessions
```

Use the returned `sessionId` to evaluate:

```bash
curl -X POST http://localhost:5000/api/sessions/{sessionId}/evaluate \
  -H "Content-Type: application/json" \
  -d '{"source":"1/2 + 1/3"}'
```

Expected result payload contains:

```json
{"canonicalText":"5/6"}
```

### React/Vite Frontend

The frontend opens directly into the evaluator workspace:

```bash
cd web/frontend
npm install
npm run dev
```

The Vite dev server proxies `/api/*` to `http://localhost:5000`. Open the
printed Vite URL, run the default input `1/2 + 1/3`, and expect `5/6`.

Frontend checks:

```bash
npm run typecheck
npm run build
```

### Docker Compose

The production-like Compose graph starts Traefik, frontend, BFF, internal
engine, and Postgres:

```bash
docker compose up --build
```

Only Traefik publishes host ports in `docker-compose.yml`:

```text
80:80
443:443
```

Routes:

```text
/      -> frontend
/api/* -> BFF
```

The engine and Postgres are on the internal Compose network and are not routed
publicly. For local debugging only, use the development override:

```bash
docker compose -f docker-compose.yml -f docker-compose.dev.yml up --build
```

The dev override additionally publishes:

```text
frontend 5173
BFF      5000
engine   8080
Postgres 5432
```

Full-slice smoke check:

1. Start Compose.
2. Open `http://localhost/`.
3. Run `1/2 + 1/3`.
4. Confirm the browser displays `5/6`.

## Build The Full Developer System

To build the kernel, packs, web API core, notebook core, CLI, SDK, examples,
and tests:

   ```bash
   cmake -S . -B build
   cmake --build build --config Release
   ```

Run all configured tests:

   ```bash
   ctest --test-dir build -C Release --output-on-failure
   ```

Focused web API verification:

   ```bash
   cmake --build build --config Release --target aleph3_web_api_tests
   ctest --test-dir build -C Release -R aleph3_web_api_tests --output-on-failure
   ```

## Use The CLI While The Web UI Is In Progress

Start the CLI REPL:

   ```bash
   ./build/bin/aleph3_cli repl
   ```

   On multi-configuration generators such as Visual Studio, the executable is
   normally under `build/bin/Release/`.

Try the symbolic surface:

   ```text
   > 1/2 + 1/3
   5/6

   > Refine[Sqrt[x^2], x >= 0]
   x

   > Replace[f[f[x], x], x -> y, {1, 2}]
   f[f[y], y]

   > PolynomialQuotient[x^2*y + x*y^2 + y, x*y, {x, y}]
   {x + y, y}

   > :inspect Factor[x^2 - 1]
   > :packs
   > :complete Pol
   > :quit
   ```

   The variable list `{x, y}` is significant: it declares variable precedence
   for multivariate division. See the manual's
   [Concepts and terminology appendix](docs/manual/concepts-and-terminology.md)
   for a plain-language explanation.

Try the trusted SDK-facing path and host functions:

   ```text
   > :validate If[True, 1, "no"]
   > :compile 1 + 2
   > :evaluate --var x=3 x + 1
   > :evaluate-host --var x=12 Clamp[x, 0, 10]
   ```

   Run `aleph3_sdk_example` for an embedding example using the C++ API.

## Build Options

For a smaller SDK-focused configuration:

   ```bash
   cmake -S . -B build-sdk -DALEPH3_BUILD_SYMBOLIC_ENGINE=OFF -DBUILD_TESTING=OFF
   cmake --build build-sdk --config Release
   ```

The SDK still uses the kernel in this configuration. The option disables the
broader symbolic surface and its tests; it does not introduce a separate
runtime.

## Architecture At A Glance

- `aleph3_kernel` owns expressions, evaluation, rewriting, exact arithmetic,
  assumptions, diagnostics, and registration.
- `aleph3_sdk` adds schemas, policies, host-facing values, and the trusted
  embedding boundary.
- math packs add domain functions through kernel registration; `core-algebra`
  currently owns the polynomial surface.
- `aleph3_web_api` is the experimental web API core over anonymous clients and
  shared sessions.
- `aleph3_cli` and the reusable session layer expose those same semantics
  interactively.
- `aleph3_notebook_core` owns the current headless document model, JSON
  persistence, and clean `Run All` lifecycle.
- the planned web frontend will own editing and presentation while delegating
  all execution to the API, session, kernel, and packs.

`aleph3_symbolic` remains a compatibility target name during migration; it is
not a second semantic engine.

## Documentation

Read the [Aleph3 Manual](docs/manual/README.md) for supported workflows and
examples. The [documentation index](docs/README.md) maps normative contracts,
architecture, product design, and the unified roadmap without duplicating that
material here.

## License
[MIT License](LICENSE)
