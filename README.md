![Build Status](https://img.shields.io/github/actions/workflow/status/sergiorf/aleph3/build.yml)
![License](https://img.shields.io/github/license/sergiorf/aleph3)

<p align="center">
  <img src="assets/logo.png" alt="Aleph3 Logo" width="200"/>
</p>

# Aleph3
Aleph3 is a C++20 engine for safely embedding formulas and evaluating a focused
symbolic-mathematics subset.

Use it to:

- validate and execute application formulas through a small SDK
- register typed host functions and control evaluation with policies and budgets
- work with exact rationals, assumptions, rewrite rules, and polynomial algebra
- reuse the same kernel semantics from the SDK, CLI, sessions, and math packs

The SDK is the most stable product surface. Symbolic functionality is real and
documented, but Aleph3 is not yet a general Mathematica-class CAS. Unsupported
algebraic forms fail explicitly instead of silently guessing.

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

## Getting Started

You need CMake 3.20+ and a C++20 compiler.

1. Clone and build:

   ```bash
   git clone https://github.com/sergiorf/aleph3.git
   cd aleph3
   cmake -S . -B build
   cmake --build build --config Release
   ```

2. Start the CLI REPL:

   ```bash
   ./build/bin/aleph3_cli repl
   ```

   On multi-configuration generators such as Visual Studio, the executable is
   normally under `build/bin/Release/`.

3. Try the symbolic surface:

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
   for multivariate division. See [Concepts and terminology](docs/concepts.md)
   for a plain-language explanation.

4. Try the trusted SDK-facing path and host functions:

   ```text
   > :validate If[True, 1, "no"]
   > :compile 1 + 2
   > :evaluate --var x=3 x + 1
   > :evaluate-host --var x=12 Clamp[x, 0, 10]
   ```

   Run `aleph3_sdk_example` for an embedding example using the C++ API.

5. Run the tests:

   ```bash
   ctest --test-dir build -C Release --output-on-failure
   ```

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
- `aleph3_cli` and the reusable session layer expose those same semantics
  interactively.

`aleph3_symbolic` remains a compatibility target name during migration; it is
not a second semantic engine.

## Documentation

Start at the [documentation index](docs/README.md). The main reading path is:

1. [Aleph3 Manual](docs/manual/README.md)
2. [Concepts and terminology](docs/concepts.md)
3. [Supported algebra subset](docs/algebra_supported_subset.md)
4. [SDK guide](docs/sdk/README.md)
5. [Architecture](docs/architecture.md)
6. [Unified implementation plan](docs/aleph3_unified_plan.md)

The index separates explanatory guides, normative contracts, and planning so
there is one obvious home for each kind of information.

## License
[MIT License](LICENSE)
