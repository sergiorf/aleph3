# Web MVP Operations

This page collects local development, service, port, Docker Compose, Traefik,
and smoke-test details for the current Web MVP slice. Product scope and
sequencing live in the [Web MVP Launch Plan](web_mvp_launch_plan.md).

## Service Path

The first browser evaluation loop uses this path:

```text
React/Vite frontend -> ASP.NET Core BFF /api/* -> internal C++ engine /internal/* -> session::Session
```

The BFF exposes the public browser API. The internal C++ engine owns symbolic
sessions, evaluation, completion, and focused help through the shared session
and kernel path. Web code must not add parser, evaluator, discovery, or pack
semantics.

## Internal C++ Engine

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

## ASP.NET Core BFF

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

## React/Vite Frontend

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

## Transitional Web API Core

The repository still includes a transport-independent legacy web API core used
as transitional contract evidence while browser traffic migrates to the BFF.
It does not start the public web MVP backend.

Build the current API-core executable:

```bash
cmake --build build --config Release --target aleph3_web_api_server
```

Launch the health-check entrypoint:

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

The implemented transitional API core is exercised through tests:

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

Focused web API verification:

```bash
cmake --build build --config Release --target aleph3_web_api_tests
ctest --test-dir build -C Release -R aleph3_web_api_tests --output-on-failure
```

## Docker Compose

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
