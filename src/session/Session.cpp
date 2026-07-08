#include "session/Session.hpp"

#include "evaluator/Evaluator.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "expr/FullForm.hpp"
#include "syntax/SymbolicLowering.hpp"
#include "transforms/Transforms.hpp"

#include <exception>
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <set>
#include <sstream>

namespace aleph3::session {

namespace {

SessionDiagnostic to_session_diagnostic(const Diagnostic& diagnostic) {
    const std::string code = diagnostic.code.starts_with("syntax.")
        ? "session.parse_error"
        : diagnostic.code;
    return SessionDiagnostic{
        code,
        diagnostic.message,
        diagnostic.severity,
        diagnostic.span};
}

std::string expression_head(const ExprPtr& expr) {
    if (const auto* call = std::get_if<FunctionCall>(expr.get())) return call->head;
    if (std::holds_alternative<Symbol>(*expr)) return "Symbol";
    if (const auto* number = std::get_if<Number>(expr.get())) {
        return std::floor(number->value) == number->value ? "Integer" : "Real";
    }
    if (std::holds_alternative<Rational>(*expr)) return "Rational";
    if (std::holds_alternative<Complex>(*expr)) return "Complex";
    if (std::holds_alternative<Boolean>(*expr)) return "Boolean";
    if (std::holds_alternative<String>(*expr)) return "String";
    if (std::holds_alternative<List>(*expr)) return "List";
    if (std::holds_alternative<Rule>(*expr)) return "Rule";
    if (std::holds_alternative<Assignment>(*expr)) return "Assignment";
    if (std::holds_alternative<FunctionDefinition>(*expr)) return "FunctionDefinition";
    return "Expression";
}

SessionInspection inspect_expression(const ExprPtr& expr) {
    SessionInspection inspection;
    inspection.head = expression_head(expr);
    inspection.full_form = to_fullform(expr);
    std::set<std::string> symbols;
    std::function<void(const ExprPtr&, std::size_t)> visit =
        [&](const ExprPtr& current, std::size_t depth) {
            if (current == nullptr) return;
            ++inspection.node_count;
            inspection.depth = std::max(inspection.depth, depth);
            std::visit([&](const auto& node) {
                using T = std::decay_t<decltype(node)>;
                if constexpr (std::is_same_v<T, Symbol>) {
                    symbols.insert(node.name);
                } else if constexpr (std::is_same_v<T, FunctionCall>) {
                    for (const auto& arg : node.args) visit(arg, depth + 1);
                } else if constexpr (std::is_same_v<T, List>) {
                    for (const auto& element : node.elements) visit(element, depth + 1);
                } else if constexpr (std::is_same_v<T, Rule>) {
                    visit(node.lhs, depth + 1); visit(node.rhs, depth + 1);
                } else if constexpr (std::is_same_v<T, Assignment>) {
                    symbols.insert(node.name); visit(node.value, depth + 1);
                } else if constexpr (std::is_same_v<T, FunctionDefinition>) {
                    symbols.insert(node.name);
                    visit(node.body, depth + 1);
                }
            }, *current);
        };
    visit(expr, 1);
    inspection.symbols.assign(symbols.begin(), symbols.end());
    return inspection;
}

}  // namespace

Session::Session() : context_(kernel::default_function_registry()) {
}

SessionResult Session::execute(const SessionRequest& request) {
    SessionResult result;
    if (request.operation != SessionOperation::discover_packs &&
        request.operation != SessionOperation::complete && request.source.empty()) {
        result.diagnostics.push_back({"session.empty_source", "An expression is required."});
        return result;
    }
    try {
        context_.reset_runtime_step_counter();
        if (request.operation == SessionOperation::discover_packs) {
            std::map<std::string, std::vector<std::string>> packages;
            for (const auto& metadata : context_.function_registry().symbolic_function_metadata()) {
                if (metadata.source == kernel::RegistrationSource::pack &&
                    !metadata.owning_package.empty()) {
                    packages[metadata.owning_package].push_back(metadata.name);
                }
            }
            for (auto& [name, symbols] : packages) {
                result.packs.push_back({name, std::move(symbols)});
            }
            result.ok = true;
            return result;
        }
        if (request.operation == SessionOperation::complete) {
            std::map<std::string, SessionCompletion> matches;
            const auto starts_with_prefix = [&](const std::string& name) {
                return name.starts_with(request.source);
            };
            for (const auto& callable : context_.function_registry().callable_metadata()) {
                if (!starts_with_prefix(callable.metadata.name)) continue;
                std::string category = "builtin";
                if (callable.metadata.source == kernel::RegistrationSource::pack) {
                    category = "pack";
                } else if (callable.category == kernel::CallableCategory::special_form) {
                    category = "special-form";
                }
                matches.emplace(callable.metadata.name, SessionCompletion{
                    callable.metadata.name,
                    std::move(category),
                    callable.metadata.owning_package,
                    callable.metadata.documentation});
            }
            for (const auto& [name, _] : context_.symbol_values.entries()) {
                if (starts_with_prefix(name)) matches[name] = {name, "symbol", "", ""};
            }
            for (const auto& [name, _] : context_.function_definitions.entries()) {
                if (starts_with_prefix(name)) matches[name] = {name, "function", "", ""};
            }
            for (auto& [_, completion] : matches) {
                result.completions.push_back(std::move(completion));
            }
            result.ok = true;
            return result;
        }
        const auto parsed_result = syntax::parse_symbolic_source(request.source);
        if (!parsed_result.ok()) {
            for (const auto& diagnostic : parsed_result.diagnostics) {
                result.diagnostics.push_back(to_session_diagnostic(diagnostic));
            }
            if (result.diagnostics.empty()) {
                result.diagnostics.push_back({"session.parse_error", "Expression parsing failed."});
            }
            return result;
        }
        const auto parsed = parsed_result.expr;
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
            case SessionOperation::inspect:
                result.inspections.push_back(inspect_expression(parsed));
                result.output = result.inspections.front().full_form;
                break;
            case SessionOperation::discover_packs:
            case SessionOperation::complete:
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
