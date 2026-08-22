/*
 * DomainRestrictions.hpp
 * ----------------------
 * Narrow metadata carrier for transformation side conditions.
 */

#pragma once

#include "expr/Expr.hpp"

#include <string>
#include <vector>

namespace aleph3::kernel {

struct DomainRestrictions {
    std::vector<ExprPtr> excluded_zero_expressions;

    void add_excluded_zero(ExprPtr expr);
    void merge(const DomainRestrictions& other);

    [[nodiscard]] bool empty() const;
    [[nodiscard]] std::vector<std::string> excluded_zero_strings() const;
};

}  // namespace aleph3::kernel
