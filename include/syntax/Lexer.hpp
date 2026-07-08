#pragma once

#include <string_view>
#include <vector>

#include "syntax/Token.hpp"

namespace aleph3::syntax {

struct LexResult {
    std::vector<Token> tokens;
    std::vector<aleph3::Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept {
        return diagnostics.empty();
    }
};

class Lexer {
public:
    explicit Lexer(std::string_view source);

    [[nodiscard]] LexResult tokenize() const;

private:
    std::string_view source_;
};

}  // namespace aleph3::syntax
