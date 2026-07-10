#include "kernel/VariableAnalysis.hpp"

#include "util/Overloaded.hpp"

#include <algorithm>
#include <type_traits>
#include <vector>

namespace aleph3::kernel {

namespace {

ExprPtr clone_expr(const ExprPtr& expr) {
    return expr == nullptr ? nullptr : std::make_shared<Expr>(*expr);
}

bool is_pattern_symbol_name(const std::string& name) {
    return name == "_" || name.find('_') != std::string::npos;
}

std::string pattern_binding_name(const std::string& name) {
    const auto separator = name.find('_');
    if (separator == 0 || separator == std::string::npos) {
        return {};
    }
    return name.substr(0, separator);
}

void insert_all(SymbolSet& target, const SymbolSet& source) {
    target.insert(source.begin(), source.end());
}

void erase_all(SymbolSet& target, const SymbolSet& names) {
    for (const auto& name : names) {
        target.erase(name);
    }
}

bool intersects(const SymbolSet& left, const SymbolSet& right) {
    for (const auto& name : left) {
        if (right.contains(name)) {
            return true;
        }
    }
    return false;
}

SymbolSet pattern_binders(const ExprPtr& expr) {
    if (expr == nullptr) {
        return {};
    }

    return std::visit(
        overloaded{
            [](const Symbol& symbol) -> SymbolSet {
                SymbolSet result;
                if (is_pattern_symbol_name(symbol.name)) {
                    const auto binding = pattern_binding_name(symbol.name);
                    if (!binding.empty()) {
                        result.insert(binding);
                    }
                }
                return result;
            },
            [](const FunctionCall& call) -> SymbolSet {
                SymbolSet result;
                for (const auto& arg : call.args) {
                    insert_all(result, pattern_binders(arg));
                }
                return result;
            },
            [](const List& list) -> SymbolSet {
                SymbolSet result;
                for (const auto& element : list.elements) {
                    insert_all(result, pattern_binders(element));
                }
                return result;
            },
            [](const Rule& rule) -> SymbolSet {
                auto result = pattern_binders(rule.lhs);
                insert_all(result, pattern_binders(rule.rhs));
                return result;
            },
            [](const FunctionDefinition& def) -> SymbolSet {
                SymbolSet result;
                for (const auto& param : def.params) {
                    result.insert(param.name);
                    insert_all(result, pattern_binders(param.default_value));
                }
                insert_all(result, pattern_binders(def.body));
                return result;
            },
            [](const Assignment& assignment) -> SymbolSet {
                return pattern_binders(assignment.value);
            },
            [](const auto&) -> SymbolSet {
                return {};
            }},
        *expr);
}

SymbolSet free_variables_impl(const ExprPtr& expr) {
    if (expr == nullptr) {
        return {};
    }

    return std::visit(
        overloaded{
            [](const Symbol& symbol) -> SymbolSet {
                if (is_pattern_symbol_name(symbol.name)) {
                    return {};
                }
                return SymbolSet{symbol.name};
            },
            [](const FunctionCall& call) -> SymbolSet {
                SymbolSet result;
                for (const auto& arg : call.args) {
                    insert_all(result, free_variables_impl(arg));
                }
                return result;
            },
            [](const List& list) -> SymbolSet {
                SymbolSet result;
                for (const auto& element : list.elements) {
                    insert_all(result, free_variables_impl(element));
                }
                return result;
            },
            [](const Rule& rule) -> SymbolSet {
                auto result = free_variables_impl(rule.lhs);
                auto rhs = free_variables_impl(rule.rhs);
                erase_all(rhs, pattern_binders(rule.lhs));
                insert_all(result, rhs);
                return result;
            },
            [](const Assignment& assignment) -> SymbolSet {
                return free_variables_impl(assignment.value);
            },
            [](const FunctionDefinition& def) -> SymbolSet {
                SymbolSet result;
                for (const auto& param : def.params) {
                    insert_all(result, free_variables_impl(param.default_value));
                }
                auto body = free_variables_impl(def.body);
                SymbolSet params;
                for (const auto& param : def.params) {
                    params.insert(param.name);
                }
                erase_all(body, params);
                insert_all(result, body);
                return result;
            },
            [](const auto&) -> SymbolSet {
                return {};
            }},
        *expr);
}

SymbolSet bound_variables_impl(const ExprPtr& expr) {
    if (expr == nullptr) {
        return {};
    }

    return std::visit(
        overloaded{
            [](const FunctionCall& call) -> SymbolSet {
                SymbolSet result;
                for (const auto& arg : call.args) {
                    insert_all(result, bound_variables_impl(arg));
                }
                return result;
            },
            [](const List& list) -> SymbolSet {
                SymbolSet result;
                for (const auto& element : list.elements) {
                    insert_all(result, bound_variables_impl(element));
                }
                return result;
            },
            [](const Rule& rule) -> SymbolSet {
                auto result = pattern_binders(rule.lhs);
                insert_all(result, bound_variables_impl(rule.lhs));
                insert_all(result, bound_variables_impl(rule.rhs));
                return result;
            },
            [](const Assignment& assignment) -> SymbolSet {
                return bound_variables_impl(assignment.value);
            },
            [](const FunctionDefinition& def) -> SymbolSet {
                SymbolSet result;
                for (const auto& param : def.params) {
                    result.insert(param.name);
                    insert_all(result, bound_variables_impl(param.default_value));
                }
                insert_all(result, bound_variables_impl(def.body));
                return result;
            },
            [](const auto&) -> SymbolSet {
                return {};
            }},
        *expr);
}

bool replacement_would_capture(
    const ExprPtr& replacement,
    const SymbolSet& bound_scope) {
    return intersects(free_variables(replacement), bound_scope);
}

ExprPtr substitute_impl(
    const ExprPtr& expr,
    const SymbolSubstitutionMap& substitutions,
    const SymbolSet& bound_scope) {
    if (expr == nullptr) {
        return nullptr;
    }

    return std::visit(
        overloaded{
            [&](const Symbol& symbol) -> ExprPtr {
                if (bound_scope.contains(symbol.name) || is_pattern_symbol_name(symbol.name)) {
                    return clone_expr(expr);
                }
                const auto found = substitutions.find(symbol.name);
                if (found == substitutions.end() ||
                    replacement_would_capture(found->second, bound_scope)) {
                    return clone_expr(expr);
                }
                return clone_expr(found->second);
            },
            [&](const FunctionCall& call) -> ExprPtr {
                std::vector<ExprPtr> args;
                args.reserve(call.args.size());
                for (const auto& arg : call.args) {
                    args.push_back(substitute_impl(arg, substitutions, bound_scope));
                }
                return make_expr<FunctionCall>(call.head, args);
            },
            [&](const List& list) -> ExprPtr {
                std::vector<ExprPtr> elements;
                elements.reserve(list.elements.size());
                for (const auto& element : list.elements) {
                    elements.push_back(substitute_impl(element, substitutions, bound_scope));
                }
                return std::make_shared<Expr>(List{elements});
            },
            [&](const Rule& rule) -> ExprPtr {
                auto lhs = substitute_impl(rule.lhs, substitutions, bound_scope);
                auto scoped = bound_scope;
                insert_all(scoped, pattern_binders(rule.lhs));
                return make_expr<Rule>(
                    lhs,
                    substitute_impl(rule.rhs, substitutions, scoped));
            },
            [&](const Assignment& assignment) -> ExprPtr {
                return make_expr<Assignment>(
                    assignment.name,
                    substitute_impl(assignment.value, substitutions, bound_scope));
            },
            [&](const FunctionDefinition& def) -> ExprPtr {
                auto scoped = bound_scope;
                for (const auto& param : def.params) {
                    scoped.insert(param.name);
                }

                auto params = def.params;
                for (auto& param : params) {
                    param.default_value = substitute_impl(param.default_value, substitutions, bound_scope);
                }
                return make_expr<FunctionDefinition>(
                    def.name,
                    params,
                    substitute_impl(def.body, substitutions, scoped),
                    def.delayed);
            },
            [&](const auto&) -> ExprPtr {
                return clone_expr(expr);
            }},
        *expr);
}

}  // namespace

SymbolSet free_variables(const ExprPtr& expr) {
    return free_variables_impl(expr);
}

SymbolSet bound_variables(const ExprPtr& expr) {
    return bound_variables_impl(expr);
}

bool depends_on(const ExprPtr& expr, const std::string& symbol_name) {
    return free_variables(expr).contains(symbol_name);
}

ExprPtr substitute_symbols_capture_safe(
    const ExprPtr& expr,
    const SymbolSubstitutionMap& substitutions) {
    return substitute_impl(expr, substitutions, {});
}

}  // namespace aleph3::kernel
