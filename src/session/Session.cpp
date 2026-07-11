#include "session/Session.hpp"

#include "evaluator/Evaluator.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "expr/FullForm.hpp"
#include "help/HelpTexts.hpp"
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

int completion_category_rank(const std::string& category) {
    if (category == "builtin") return 0;
    if (category == "special-form") return 1;
    if (category == "pack") return 2;
    if (category == "function") return 3;
    if (category == "symbol") return 4;
    return 5;
}

std::string callable_category_name(const kernel::CallableMetadata& callable) {
    if (callable.metadata.source == kernel::RegistrationSource::pack) {
        return "pack";
    }
    if (callable.category == kernel::CallableCategory::special_form) {
        return "special-form";
    }
    return "builtin";
}

std::string completion_documentation_for(const std::string& name, const std::string& fallback) {
    if (!fallback.empty()) {
        return fallback;
    }
    if (const auto* help = find_help_entry(name)) {
        return help->description;
    }
    return "";
}

SessionHelpEntry make_help_entry(
    std::string name,
    std::string category,
    std::string owning_package,
    std::string documentation = {}) {
    SessionHelpEntry entry;
    entry.name = std::move(name);
    entry.category = std::move(category);
    entry.owning_package = std::move(owning_package);
    if (const auto* help = find_help_entry(entry.name)) {
        entry.description = help->description;
        entry.forms = help->forms;
        entry.examples = help->examples;
        entry.exactness = help->exactness;
        entry.unsupported = help->unsupported;
        entry.manual_anchor = help->manual_anchor;
        if (entry.owning_package.empty() && !help->owning_component.empty() &&
            help->owning_component != "builtin" && help->owning_component != "syntax" &&
            help->owning_component != "session") {
            entry.owning_package = help->owning_component;
        }
    }
    if (entry.description.empty()) {
        entry.description = std::move(documentation);
    }
    return entry;
}

bool help_matches_query(const SessionHelpEntry& entry, const std::string& query, bool exact_mode) {
    if (query.empty()) return true;
    if (exact_mode) {
        return entry.name == query || entry.owning_package == query || entry.category == query;
    }
    return entry.name.starts_with(query) ||
        (!entry.owning_package.empty() && entry.owning_package.starts_with(query)) ||
        entry.category.starts_with(query);
}

}  // namespace

Session::Session() : context_(kernel::default_function_registry()) {
}

void Session::reset() {
    const auto& registry = context_.function_registry();
    context_ = kernel::EvaluationContext(registry);
}

SessionResult Session::execute(const SessionRequest& request) {
    SessionResult result;
    if (request.operation != SessionOperation::discover_packs &&
        request.operation != SessionOperation::complete &&
        request.operation != SessionOperation::help &&
        request.source.empty()) {
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
                auto category = callable_category_name(callable);
                matches.emplace(callable.metadata.name, SessionCompletion{
                    callable.metadata.name,
                    std::move(category),
                    callable.metadata.owning_package,
                    completion_documentation_for(callable.metadata.name, callable.metadata.documentation)});
            }
            for (const auto& [name, _] : context_.symbol_values.entries()) {
                if (starts_with_prefix(name)) {
                    matches.emplace(name, SessionCompletion{name, "symbol", "", "session-local own value"});
                }
            }
            for (const auto& [name, _] : context_.function_definitions.entries()) {
                if (!starts_with_prefix(name)) continue;
                auto it = matches.find(name);
                if (it == matches.end() || it->second.category == "symbol") {
                    matches[name] = {name, "function", "", "session-local user function"};
                }
            }
            for (auto& [_, completion] : matches) {
                result.completions.push_back(std::move(completion));
            }
            std::sort(result.completions.begin(), result.completions.end(), [](const auto& left, const auto& right) {
                if (left.name != right.name) return left.name < right.name;
                return completion_category_rank(left.category) < completion_category_rank(right.category);
            });
            result.ok = true;
            return result;
        }
        if (request.operation == SessionOperation::help) {
            std::map<std::string, SessionHelpEntry> candidates;
            const auto add_candidate = [&](SessionHelpEntry entry) {
                candidates.emplace(entry.name, std::move(entry));
            };
            for (const auto& callable : context_.function_registry().callable_metadata()) {
                add_candidate(make_help_entry(
                    callable.metadata.name,
                    callable_category_name(callable),
                    callable.metadata.owning_package,
                    callable.metadata.documentation));
            }
            for (const auto& entry : get_help_entries()) {
                add_candidate(make_help_entry(entry.name, entry.category, entry.owning_component, entry.description));
            }
            for (const auto& [name, _] : context_.symbol_values.entries()) {
                add_candidate(SessionHelpEntry{name, "symbol", "", "session-local own value"});
            }
            for (const auto& [name, _] : context_.function_definitions.entries()) {
                auto entry = SessionHelpEntry{name, "function", "", "session-local user function"};
                auto it = candidates.find(name);
                if (it == candidates.end() || it->second.category == "symbol") {
                    candidates[name] = std::move(entry);
                }
            }
            const bool exact_mode = std::any_of(
                candidates.begin(),
                candidates.end(),
                [&](const auto& item) {
                    const auto& entry = item.second;
                    return entry.name == request.source ||
                        entry.owning_package == request.source ||
                        entry.category == request.source;
                });
            for (auto& [_, entry] : candidates) {
                if (help_matches_query(entry, request.source, exact_mode)) {
                    result.help_entries.push_back(std::move(entry));
                }
            }
            std::sort(result.help_entries.begin(), result.help_entries.end(), [](const auto& left, const auto& right) {
                if (left.name != right.name) return left.name < right.name;
                return completion_category_rank(left.category) < completion_category_rank(right.category);
            });
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
            case SessionOperation::help:
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
