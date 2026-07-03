#pragma once

#include <string>
#include <string_view>

#include "session/Session.hpp"

namespace aleph3::tooling {

struct SymbolicCliResult {
    bool ok = false;
    std::string output;
    std::string error_message;
};

SymbolicCliResult symbolic_evaluate_expression(std::string_view source);
SymbolicCliResult symbolic_evaluate_expression(
    std::string_view source,
    session::Session& session);

SymbolicCliResult symbolic_simplify_expression(std::string_view source);

SymbolicCliResult symbolic_fullform_expression(std::string_view source);

}  // namespace aleph3::tooling
