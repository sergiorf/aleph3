![Build Status](https://img.shields.io/github/actions/workflow/status/sergiorf/mathix/build.yml)
![License](https://img.shields.io/github/license/sergiorf/mathix)

<p align="center">
  <img src="assets/logo.png" alt="Mathix Logo" width="200"/>
</p>

# Mathix
_A symbolic math system and computational software written in C++._

## ✨Overview

Mathix is a powerful and flexible computer algebra system (CAS) written in modern C++ (C++20). It enables users to perform symbolic computations, manipulate mathematical expressions, and define custom functions.

## Features
- Basic algebraic operations: `+`, `-`, `*`, `/`
- Symbolic computation support
- Extensible architecture for advanced features

## Getting Started
To build Mathix, ensure you have CMake 3.20+ and a C++20-compatible compiler installed. Follow these steps:
1. Clone the repository:
   ```bash
   git clone https://github.com/sergiorf/mathix.git
   cd mathix
   ```

2. Build the project:
   ```bash
   cmake -S . -B build
   cmake --build build
   cd build
   ctest -C Debug --output-on-failure --verbose
   ```

3. Run the Mathix CLI:
   ```bash
   ./build/bin/mathix
   ```

## Looking for Contributors 🚀
We are actively looking for contributors to help improve Mathix! Whether you're experienced in C++ or just starting out, your contributions are welcome. Here are some ways you can help:
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