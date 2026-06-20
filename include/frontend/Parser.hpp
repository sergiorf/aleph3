#pragma once

#include <string_view>
#include <vector>

#include "frontend/Lexer.hpp"
#include "ir/Node.hpp"

namespace aleph3::frontend {

struct ParseResult {
    ir::NodePtr root;
    std::vector<aleph3::Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept {
        return root != nullptr && diagnostics.empty();
    }
};

class Parser {
public:
    explicit Parser(std::string_view source);

    [[nodiscard]] ParseResult parse() const;

private:
    std::string_view source_;
};

}  // namespace aleph3::frontend
