![Build Status](https://img.shields.io/github/actions/workflow/status/sergiorf/aleph3/build.yml)
![License](https://img.shields.io/github/license/sergiorf/aleph3)

<p align="center">
  <img src="assets/logo.png" alt="Aleph3 Logo" width="200"/>
</p>

# Aleph3
Aleph3 is transitioning from a broad CAS prototype toward a smaller,
embeddable formula engine for native applications. The legacy parser/evaluator
still exist in this repository, but the current rewrite direction is SDK-first:
stable public types, a narrow trusted-subset IR, structured diagnostics, and
host-controlled evaluation.

## Current Repository Tracks
- Legacy prototype engine: broad parser/evaluator behavior used as reference material
- Rewrite path: public SDK under `include/sdk/` and trusted-subset IR under `include/ir/`
- Rewrite tooling CLI: `aleph3_rewrite_cli` for manual lexer/parser/SDK checks
- Rewrite docs index: [docs/rewrite/README.md](docs/rewrite/README.md)

## Getting Started
To build Aleph3, ensure you have CMake 3.20+ and a C++20-compatible compiler installed. Follow these steps:
1. Clone the repository:
   ```bash
   git clone https://github.com/sergiorf/aleph3.git
   cd aleph3
   ```

2. Build the rewrite SDK only:
   ```bash
   cmake -S . -B build -DALEPH3_BUILD_LEGACY=OFF -DBUILD_TESTING=OFF
   cmake --build build
   ```

   Run the rewrite CLI:
   ```bash
   ./build/bin/aleph3_rewrite_cli
   ./build/bin/aleph3_rewrite_cli help
   ./build/bin/aleph3_rewrite_cli examples
   ./build/bin/aleph3_rewrite_cli tokens "If[x >= 1, \"ok\", False]"
   ./build/bin/aleph3_rewrite_cli parse "2 + 3 * (x + 1)"
   ./build/bin/aleph3_rewrite_cli validate "x + 1"
   ./build/bin/aleph3_rewrite_cli compile "x + 1"
   ./build/bin/aleph3_rewrite_cli repl
   ```

   Running `./build/bin/aleph3_rewrite_cli` with no arguments starts the interactive REPL.

3. Build the legacy CLI and tests when you need the prototype path:
   ```bash
   cmake -S . -B build
   cmake --build build
   cd build
   ctest --output-on-failure --verbose
   ```

## Documentation
- Rewrite overview: [docs/rewrite/README.md](docs/rewrite/README.md)
- Controlled rewrite plan: [docs/embedded_formula_engine_implementation_plan.md](docs/embedded_formula_engine_implementation_plan.md)
- System architecture: [docs/system_architecture.md](docs/system_architecture.md)
- Trusted subset scope: [docs/trusted_subset_v1.md](docs/trusted_subset_v1.md)
- Style guide: [docs/style_guide.md](docs/style_guide.md)

## License
[MIT License](LICENSE)
