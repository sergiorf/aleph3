#include "kernel/DomainRestrictions.hpp"

#include <algorithm>
#include <utility>

namespace aleph3::kernel {

namespace {

bool same_expression_text(const ExprPtr& left, const ExprPtr& right) {
    return to_string(left) == to_string(right);
}

}  // namespace

void DomainRestrictions::add_excluded_zero(ExprPtr expr) {
    if (!expr) return;
    const auto duplicate = std::find_if(
        excluded_zero_expressions.begin(),
        excluded_zero_expressions.end(),
        [&](const ExprPtr& existing) {
            return same_expression_text(existing, expr);
        });
    if (duplicate != excluded_zero_expressions.end()) return;

    excluded_zero_expressions.push_back(std::move(expr));
    std::sort(
        excluded_zero_expressions.begin(),
        excluded_zero_expressions.end(),
        [](const ExprPtr& left, const ExprPtr& right) {
            return to_string(left) < to_string(right);
        });
}

void DomainRestrictions::merge(const DomainRestrictions& other) {
    for (const auto& expr : other.excluded_zero_expressions) {
        add_excluded_zero(expr);
    }
}

bool DomainRestrictions::empty() const {
    return excluded_zero_expressions.empty();
}

std::vector<std::string> DomainRestrictions::excluded_zero_strings() const {
    std::vector<std::string> result;
    result.reserve(excluded_zero_expressions.size());
    for (const auto& expr : excluded_zero_expressions) {
        result.push_back(to_string(expr));
    }
    return result;
}

}  // namespace aleph3::kernel
