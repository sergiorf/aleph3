#include "session/Session.hpp"

#include "evaluator/Evaluator.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "expr/FullForm.hpp"
#include "parser/Parser.hpp"
#include "transforms/Transforms.hpp"

#include <exception>

namespace aleph3::session {

Session::Session() : context_(kernel::default_function_registry()) {
}

SessionResult Session::execute(const SessionRequest& request) {
    SessionResult result;
    if (request.source.empty()) {
        result.diagnostics.push_back({"session.empty_source", "An expression is required."});
        return result;
    }
    try {
        context_.reset_runtime_step_counter();
        const auto parsed = parse_expression(request.source);
        switch (request.operation) {
            case SessionOperation::evaluate:
                result.output = to_string(evaluate(parsed, context_));
                break;
            case SessionOperation::simplify:
                result.output = to_string(simplify(evaluate(parsed, context_)));
                break;
            case SessionOperation::full_form:
                result.output = to_fullform(parsed);
                break;
        }
        result.ok = true;
    } catch (const kernel::RuntimeFailure& failure) {
        result.diagnostics.push_back({failure.error().code, failure.what()});
    } catch (const EvaluatorError& error) {
        result.diagnostics.push_back({std::string(error.code_string()), error.what()});
    } catch (const std::exception& error) {
        result.diagnostics.push_back({"session.parse_error", error.what()});
    }
    return result;
}

}  // namespace aleph3::session
