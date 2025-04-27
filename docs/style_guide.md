# C++ Style Guide for Mathix

This guide outlines the coding conventions and best practices for contributing to Mathix. Following these guidelines ensures consistency and readability across the codebase.

---

## General Guidelines
- **Use Modern C++**: Leverage C++20 features where appropriate, such as `std::variant`, `std::visit`, and smart pointers.
- **Keep Code Simple**: Write clear and concise code. Avoid unnecessary complexity.

---

## Naming Conventions
- **File Names**: Use `CamelCase` for file names.
  - Example: `EvaluationContext.hpp`, `FunctionCall.cpp`
- **Class Names**: Use `CamelCase` for class names.
  - Example: `EvaluationContext`, `FunctionCall`
- **Variable Names**: Use `snake_case` for variable and function names.
  - Example: `evaluation_context`, `make_expr`
- **Constants**: Use `UPPER_SNAKE_CASE` for constants.
  - Example: `MAX_BUFFER_SIZE`

---

## Code Formatting
- **Indentation**: Use 4 spaces for indentation. Do not use tabs.
- **Line Length**: Limit lines to 80-100 characters for better readability.
- **Braces**: Place opening braces on the same line as the statement.
  ```cpp
  if (condition) {
      // Code block
  } else {
      // Code block
  }
  ```

---

## Comments
- **Use Doxygen-Style Comments**: Write comments for classes, functions, and complex logic.
  ```cpp
  /**
   * Evaluates a mathematical expression.
   * @param expr The expression to evaluate.
   * @param ctx The evaluation context.
   * @return The result of the evaluation.
   */
  ExprPtr evaluate(const ExprPtr& expr, EvaluationContext& ctx);
  ```
- **Avoid Redundant Comments**: Only comment on non-obvious code.

---

## Best Practices
- **Smart Pointers**: Use `std::shared_ptr` or `std::unique_ptr` for dynamic memory management. Avoid raw pointers.
- **Error Handling**: Use exceptions for error handling. Avoid returning error codes.
  ```cpp
  if (rnum.value == 0.0) {
      throw std::runtime_error("Division by zero");
  }
  ```
- **Const-Correctness**: Use `const` wherever possible to indicate immutability.
  ```cpp
  const std::string& get_name() const;
  ```

---

## Example Code
Here’s an example of properly styled code:
```cpp
#include "EvaluationContext.hpp"
#include <stdexcept>
#include <iostream>

/**
 * Represents a mathematical function call.
 */
struct FunctionCall {
    std::string head;
    std::vector<ExprPtr> args;

    FunctionCall(const std::string& h, const std::vector<ExprPtr>& a)
        : head(h), args(a) {}
};

/**
 * Evaluates a function call.
 */
ExprPtr evaluate_function(const FunctionCall& func, EvaluationContext& ctx) {
    if (func.head == "Plus") {
        auto left = evaluate(func.args[0], ctx);
        auto right = evaluate(func.args[1], ctx);
        auto lnum = std::get<Number>(*left);
        auto rnum = std::get<Number>(*right);
        return make_expr<Number>(lnum.value + rnum.value);
    }
    throw std::runtime_error("Unknown function: " + func.head);
}
```

---

## Additional Notes
- Follow the existing code style in the repository.
- Run code through a linter or formatter before submitting a pull request.

---

By adhering to this style guide, you help maintain a clean and consistent codebase for Mathix. Thank you for contributing!