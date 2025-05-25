![Build Status](https://img.shields.io/github/actions/workflow/status/sergiorf/aleph3/build.yml)
![License](https://img.shields.io/github/license/sergiorf/aleph3)

<p align="center">
  <img src="assets/logo.png" alt="Aleph3 Logo" width="200"/>
</p>

# Aleph3
_A symbolic math system and computational software written in C++._

## ✨Overview

Aleph3 is a powerful and flexible computer algebra system (CAS) written in modern C++ (C++20). It enables users to perform symbolic computations, manipulate mathematical expressions, and define custom functions.

## Features
- Basic algebraic operations: `+`, `-`, `*`, `/`
- Symbolic computation support
- Extensible architecture for advanced features

## Getting Started
To build Aleph3, ensure you have CMake 3.20+ and a C++20-compatible compiler installed. Follow these steps:
1. Clone the repository:
   ```bash
   git clone https://github.com/sergiorf/aleph3.git
   cd aleph3
   ```

2. Build the project:
   ```bash
   cmake -S . -B build
   cmake --build build
   cd build
   ctest -C Debug --output-on-failure --verbose
   ```

3. Run the Aleph3 CLI:
   ```bash
   ./build/bin/aleph3
   ```

## Looking for Contributors 🚀
We are actively looking for contributors to help improve Aleph3! Whether you're experienced in C++ or just starting out, your contributions are welcome. Here are some ways you can help:
- Add new features (e.g., advanced mathematical operations, symbolic differentiation).
- Improve the codebase by refactoring or optimizing existing code.
- Write documentation or tutorials for new users.
- Report bugs or suggest enhancements.

If you're interested, please:
1. Open an issue or discussion in the repository.
2. Contact me via email for more details.

Check out the [C++ Style Guide](docs/style_guide.md) to get started with contributing!

## License
[MIT License](LICENSE)