#include "expr/ExprStructural.hpp"

#include <cstddef>
#include <type_traits>

namespace aleph3 {

namespace {

bool structural_equal_list(
    const std::vector<ExprPtr>& left,
    const std::vector<ExprPtr>& right) noexcept;

bool structural_equal_impl(const Expr& left, const Expr& right) noexcept {
    if (left.index() != right.index()) {
        return false;
    }

    return std::visit(
        [&](const auto& lhs) noexcept -> bool {
            using T = std::decay_t<decltype(lhs)>;
            const auto& rhs = std::get<T>(right);

            if constexpr (std::is_same_v<T, Symbol>) {
                return lhs.name == rhs.name;
            } else if constexpr (std::is_same_v<T, Number>) {
                return lhs.value == rhs.value;
            } else if constexpr (std::is_same_v<T, Complex>) {
                return lhs.real == rhs.real && lhs.imag == rhs.imag;
            } else if constexpr (std::is_same_v<T, Rational>) {
                return lhs.numerator == rhs.numerator &&
                       lhs.denominator == rhs.denominator;
            } else if constexpr (std::is_same_v<T, Boolean>) {
                return lhs.value == rhs.value;
            } else if constexpr (std::is_same_v<T, String>) {
                return lhs.value == rhs.value;
            } else if constexpr (std::is_same_v<T, FunctionCall>) {
                return lhs.head == rhs.head &&
                       structural_equal_list(lhs.args, rhs.args);
            } else if constexpr (std::is_same_v<T, FunctionDefinition>) {
                if (lhs.name != rhs.name ||
                    lhs.delayed != rhs.delayed ||
                    lhs.params.size() != rhs.params.size()) {
                    return false;
                }
                for (std::size_t index = 0; index < lhs.params.size(); ++index) {
                    if (lhs.params[index].name != rhs.params[index].name) {
                        return false;
                    }
                    if (!structural_equal(lhs.params[index].default_value,
                                          rhs.params[index].default_value)) {
                        return false;
                    }
                }
                return structural_equal(lhs.body, rhs.body);
            } else if constexpr (std::is_same_v<T, Assignment>) {
                return lhs.name == rhs.name &&
                       structural_equal(lhs.value, rhs.value);
            } else if constexpr (std::is_same_v<T, Rule>) {
                return structural_equal(lhs.lhs, rhs.lhs) &&
                       structural_equal(lhs.rhs, rhs.rhs);
            } else if constexpr (std::is_same_v<T, List>) {
                return structural_equal_list(lhs.elements, rhs.elements);
            } else if constexpr (std::is_same_v<T, Infinity> ||
                                 std::is_same_v<T, ComplexInfinity> ||
                                 std::is_same_v<T, Indeterminate>) {
                return true;
            } else {
                return false;
            }
        },
        left);
}

bool structural_equal_list(
    const std::vector<ExprPtr>& left,
    const std::vector<ExprPtr>& right) noexcept {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (!structural_equal(left[index], right[index])) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool structural_equal(const ExprPtr& lhs, const ExprPtr& rhs) noexcept {
    if (lhs == rhs) {
        return true;
    }
    if (lhs == nullptr || rhs == nullptr) {
        return lhs == nullptr && rhs == nullptr;
    }
    return structural_equal_impl(*lhs, *rhs);
}

}  // namespace aleph3
