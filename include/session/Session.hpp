/* Experimental stateful kernel session for interactive clients. */
#pragma once
#include <string>
#include <cstddef>
#include <vector>
#include "kernel/EvaluationContext.hpp"
#include "sdk/Types.hpp"
namespace aleph3::session {
enum class SessionOperation { evaluate, simplify, full_form, inspect, discover_packs, complete, help };
struct SessionRequest { std::string source; SessionOperation operation = SessionOperation::evaluate; };
struct SessionDiagnostic {
    std::string code;
    std::string message;
    DiagnosticSeverity severity = DiagnosticSeverity::error;
    SourceSpan span;
};
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
struct SessionHelpEntry {
    std::string name;
    std::string category;
    std::string owning_package;
    std::string description;
    std::vector<std::string> forms;
    std::vector<std::string> examples;
    std::string exactness;
    std::string unsupported;
    std::string manual_anchor;
};
struct SessionResult {
    bool ok = false;
    std::string output;
    std::vector<SessionDiagnostic> diagnostics;
    std::vector<SessionInspection> inspections;
    std::vector<SessionPack> packs;
    std::vector<SessionCompletion> completions;
    std::vector<SessionHelpEntry> help_entries;
};
class Session {
public:
    Session();
    [[nodiscard]] SessionResult execute(const SessionRequest& request);
    void reset();
private:
    kernel::EvaluationContext context_;
};
}  // namespace aleph3::session
