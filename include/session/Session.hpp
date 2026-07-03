/* Experimental stateful kernel session for interactive clients. */
#pragma once
#include <string>
#include <cstddef>
#include <vector>
#include "kernel/EvaluationContext.hpp"
namespace aleph3::session {
enum class SessionOperation { evaluate, simplify, full_form, inspect, discover_packs, complete };
struct SessionRequest { std::string source; SessionOperation operation = SessionOperation::evaluate; };
struct SessionDiagnostic { std::string code; std::string message; };
struct SessionInspection {
    std::string head;
    std::string full_form;
    std::vector<std::string> symbols;
    std::size_t node_count = 0;
    std::size_t depth = 0;
};
struct SessionPack { std::string name; std::vector<std::string> symbols; };
struct SessionCompletion {
    std::string name;
    std::string category;
    std::string owning_package;
    std::string documentation;
};
struct SessionResult {
    bool ok = false;
    std::string output;
    std::vector<SessionDiagnostic> diagnostics;
    std::vector<SessionInspection> inspections;
    std::vector<SessionPack> packs;
    std::vector<SessionCompletion> completions;
};
class Session {
public:
    Session();
    [[nodiscard]] SessionResult execute(const SessionRequest& request);
private:
    kernel::EvaluationContext context_;
};
}  // namespace aleph3::session
