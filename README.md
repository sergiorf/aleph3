![Build Status](https://img.shields.io/github/actions/workflow/status/sergiorf/aleph3/build.yml)
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
- run the CLI as the current interactive local fallback
- validate and execute application formulas through a small SDK
- work with exact rationals, assumptions, rewrite rules, and polynomial algebra
- keep web, CLI, SDK, sessions, notebook documents, and math packs on one
  semantic path

The web API target is new and intentionally narrow. It is currently a
transport-independent API core plus a health-check executable, not yet a full
HTTP listener, browser frontend, SQLite-backed notebook store, or production
deployment. The CLI remains the easiest way to interact with the symbolic
system manually while the web product is being assembled.

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
start a listening HTTP server yet. The implemented API core is exercised
through tests and currently covers:

```text
GET  /api/health
POST /api/clients
POST /api/sessions
GET  /api/sessions/{sessionId}
POST /api/sessions/{sessionId}/evaluate
POST /api/sessions/{sessionId}/reset
DELETE /api/sessions/{sessionId}
```

Session endpoints use anonymous client ownership and delegate evaluation to
`session::Session`; web code does not add symbolic parser, evaluator, or pack
semantics.

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
