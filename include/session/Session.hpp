/* Experimental stateful kernel session for interactive clients. */
#pragma once
#include <string>
#include <vector>
#include "kernel/EvaluationContext.hpp"
namespace aleph3::session {
enum class SessionOperation { evaluate, simplify, full_form };
struct SessionRequest { std::string source; SessionOperation operation = SessionOperation::evaluate; };
struct SessionDiagnostic { std::string code; std::string message; };
struct SessionResult { bool ok = false; std::string output; std::vector<SessionDiagnostic> diagnostics; };
class Session {
public:
    Session();
    [[nodiscard]] SessionResult execute(const SessionRequest& request);
private:
    kernel::EvaluationContext context_;
};
}  // namespace aleph3::session
