![CI Status](https://img.shields.io/github/actions/workflow/status/sergiorf/aleph3/build.yml?label=CI)
![License](https://img.shields.io/github/license/sergiorf/aleph3)

<p align="center">
  <img src="assets/logo.png" alt="Aleph3 Logo" width="200"/>
</p>

# Aleph3

Aleph3 is a modern C++ symbolic mathematics engine and interactive notebook
project for exact computation, algebra, expression manipulation, and
extensible mathematical computing.

The symbolic kernel is the core of the project. The CLI, SDK, session layer,
web API, and notebook-facing interfaces are intended to share that same
semantic engine rather than implement separate evaluator behavior.

Aleph3 is under active development. APIs and product surfaces may still evolve,
and the supported symbolic subset is deliberately smaller than systems such as
Mathematica, Maple, SageMath, or SymPy. Unsupported operations should produce
explicit diagnostics instead of silently guessing.

## Quick Examples

The CLI and session-backed surfaces currently accept a Wolfram-like expression
syntax:

```text
1/2 + 1/3
Factor[x^2 - 1]
Refine[Sqrt[x^2], x >= 0]
PolynomialQuotient[x^2*y + x*y^2 + y, x*y, {x, y}]
```

In the REPL:

```text
> 1/2 + 1/3
5/6

> Factor[x^2 - 1]
(x - 1) * (x + 1)

> Refine[Sqrt[x^2], x >= 0]
x

> PolynomialQuotient[x^2*y + x*y^2 + y, x*y, {x, y}]
{x + y, y}
```

The syntax is a current frontend, not the whole product identity. The kernel
keeps syntax separate from expression meaning so compatibility syntax,
Aleph3-native syntax, or both can evolve over time.

## What Works Today

Current implemented surfaces include:

- a C++20 symbolic kernel with exact rationals, expression evaluation,
  assumptions, diagnostics, rewrite support, and function registration;
- a CLI workbench over the shared session, kernel, and registered packs;
- a C++ SDK path for validating and executing application formulas through the
  shared kernel;
- algebra pack functionality including focused polynomial operations;
- a headless notebook core for document structure, JSON persistence, and clean
  `Run All` lifecycle behavior;
- an internal C++ engine service and transitional web API core over shared
  sessions;
- an ASP.NET Core BFF skeleton and React/Vite frontend slice for the Web MVP.

The CLI is currently the fastest way to try the engine locally. The web layer
is being assembled around this path:

```text
React/Vite frontend -> ASP.NET Core BFF /api/* -> internal C++ engine /internal/* -> session::Session -> kernel + packs
```

## Build And Try The CLI

You need CMake 3.20+ and a C++20 compiler.

```bash
git clone https://github.com/sergiorf/aleph3.git
cd aleph3
cmake -S . -B build
cmake --build build --config Release --target aleph3_cli
```

Run the REPL on Unix-like single-configuration builds:

```bash
./build/bin/aleph3_cli repl
```

Run the REPL on Windows or other multi-configuration builds:

```powershell
.\build\bin\Release\aleph3_cli.exe repl
```

One-shot evaluation also works:

```bash
./build/bin/aleph3_cli "Factor[x^2 - 1]"
```

```powershell
.\build\bin\Release\aleph3_cli.exe "Factor[x^2 - 1]"
```

Run stateful scripts with one expression per non-empty line:

```bash
./build/bin/aleph3_cli script calculations.aleph3
./build/bin/aleph3_cli script --json calculations.aleph3
```

Useful REPL commands:

```text
> :help
> :help Factor
> :packs
> :complete Pol
> :inspect Factor[x^2 - 1]
> :reset
> :quit
```

On Windows and Unix-like interactive terminals, Tab completes commands and
symbol names, arrows navigate history, and left/right arrows edit the current
line. `:complete <prefix>` is the portable fallback and is also useful in
piped tests.

## Build The Developer System

Build the kernel, packs, CLI, SDK, notebook core, web components, examples,
and tests:

```bash
cmake -S . -B build
cmake --build build --config Release
```

Run the configured CTest suite:

```bash
ctest --test-dir build -C Release --output-on-failure
```

For a smaller SDK-focused configuration:

```bash
cmake -S . -B build-sdk -DALEPH3_BUILD_SYMBOLIC_ENGINE=OFF -DBUILD_TESTING=OFF
cmake --build build-sdk --config Release
```

The SDK still uses the kernel in this configuration. The option disables the
broader symbolic product surface and its tests; it does not introduce a
separate runtime.

## Web MVP Slice

The current web implementation uses ASP.NET Core for the BFF and React/Vite
for the browser frontend. The C++ engine service owns symbolic sessions and
delegates computation to the same session, kernel, and pack code used by the
CLI.

Build and smoke-test the internal engine service:

```bash
cmake --build build --config Release --target aleph3_engine_service
./build/bin/aleph3_engine_service --health
```

Start the BFF and frontend for local development:

```bash
cd web/bff
dotnet run
```

```bash
cd web/frontend
npm install
npm run dev
```

Detailed service endpoints, ports, Docker Compose topology, Traefik routing,
and smoke-test procedures live in
[Web MVP Operations](docs/web_mvp_operations.md). Launch scope and sequencing
live in the [Web MVP Launch Plan](docs/web_mvp_launch_plan.md).

## Architecture At A Glance

- `aleph3_kernel` owns expressions, evaluation, rewriting, exact arithmetic,
  assumptions, diagnostics, resource budgets, and registration.
- Math packs add domain functions through kernel registration; `core-algebra`
  currently owns the focused polynomial surface.
- `aleph3_sdk` adds schemas, policies, host-facing values, and the trusted
  embedding boundary over the kernel.
- The reusable session layer owns interactive state and exposes evaluation,
  completion, help, and reset behavior to CLI and web consumers.
- `aleph3_cli` is the local interactive and scripting workbench.
- `aleph3_notebook_core` owns the current headless document model,
  persistence, and clean `Run All` lifecycle.
- The web product path uses React/Vite, an ASP.NET Core BFF, and an internal
  C++ engine service; browser and BFF code must not add private symbolic
  semantics.

`aleph3_symbolic` remains a compatibility target name during migration; it is
not a second semantic engine.

For the full ownership model, see [Architecture](docs/architecture.md).

## Project Status

Aleph3 is an open-source personal engineering project in active development.
The strongest current surfaces are the kernel, CLI, SDK, sessions, focused
algebra support, notebook core, and the first web evaluation path.

The near-term product work is the Web MVP: a usable browser notebook backed by
the shared semantic engine. Broader CAS features, richer notebook UX, wider
calculus, solving, plotting, arbitrary-precision expansion, DSP packs, and
large compatibility claims remain future work unless documented as supported
in the manual and specifications.

GitHub Actions runs the `CI` workflow for pushes and pull requests targeting
`main`. The workflow builds and runs the CTest suite on Ubuntu and Windows,
and checks changed C++ source/header formatting with `clang-format` on Ubuntu.

## Documentation

- [Aleph3 Manual](docs/manual/README.md) - supported workflows and examples.
- [Documentation Index](docs/README.md) - canonical documentation map.
- [Architecture](docs/architecture.md) - system shape and ownership
  boundaries.
- [SDK Guide](docs/sdk/README.md) - embedding surface and SDK references.
- [Web MVP Launch Plan](docs/web_mvp_launch_plan.md) - current web product
  scope and sequencing.
- [Unified Plan](docs/aleph3_unified_plan.md) - longer-term implementation
  roadmap.

The variable list `{x, y}` in multivariate algebra declares variable
precedence. See the manual's
[Concepts and terminology appendix](docs/manual/concepts-and-terminology.md)
for a plain-language explanation.

## License

Copyright 2025 Sergio Rodriguez Freire.

Aleph3 is licensed under the [Apache License 2.0](LICENSE).
