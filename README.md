![Build Status](https://img.shields.io/github/actions/workflow/status/sergiorf/aleph3/build.yml)
![License](https://img.shields.io/github/license/sergiorf/aleph3)

<p align="center">
  <img src="assets/logo.png" alt="Aleph3 Logo" width="200"/>
</p>

# Aleph3
aleph3 is a modern, extensible computer algebra system written in C++20. It provides fast symbolic computation and mathematical expression evaluation, with a focus on clarity, performance, and a familiar, Mathematica-inspired syntax.

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