#pragma once

#include <string_view>
#include <vector>

#include "syntax/Lexer.hpp"
#include "syntax/Node.hpp"

namespace aleph3::syntax {

struct ParserOptions {
    bool allow_implicit_multiplication = false;
    bool parse_exact_rational_literals = false;
};

struct ParseResult {
    NodePtr root;
    std::vector<aleph3::Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept {
        return root != nullptr && diagnostics.empty();
    }
};

class Parser {
public:
    explicit Parser(std::string_view source, ParserOptions options = {});

    [[nodiscard]] ParseResult parse() const;

private:
    std::string_view source_;
    ParserOptions options_;
};

}  // namespace aleph3::syntax
