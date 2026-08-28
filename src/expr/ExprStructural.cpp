#include "expr/ExprStructural.hpp"

#include <cstddef>
#include <functional>
#include <type_traits>

namespace aleph3 {

namespace {

bool structural_equal_list(
    const std::vector<ExprPtr>& left,
    const std::vector<ExprPtr>& right) noexcept;

template <typename T>
void hash_combine(std::size_t& seed, const T& value) noexcept {
    seed ^= std::hash<T>{}(value) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}

std::size_t hash_double(double value) noexcept {
    if (value == 0.0) {
        value = 0.0;
    }
    return std::hash<double>{}(value);
}

std::size_t structural_hash_list(const std::vector<ExprPtr>& values) noexcept;
std::size_t structural_hash_impl(const Expr& expr) noexcept;

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

std::size_t structural_hash_list(const std::vector<ExprPtr>& values) noexcept {
    std::size_t seed = values.size();
    for (const auto& value : values) {
        hash_combine(seed, structural_hash(value));
    }
    return seed;
}

std::size_t structural_hash_impl(const Expr& expr) noexcept {
    std::size_t seed = expr.index();

    std::visit(
        [&](const auto& value) noexcept {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<T, Symbol>) {
                hash_combine(seed, value.name);
            } else if constexpr (std::is_same_v<T, Number>) {
                hash_combine(seed, hash_double(value.value));
            } else if constexpr (std::is_same_v<T, Complex>) {
                hash_combine(seed, hash_double(value.real));
                hash_combine(seed, hash_double(value.imag));
            } else if constexpr (std::is_same_v<T, Rational>) {
                hash_combine(seed, value.numerator);
                hash_combine(seed, value.denominator);
            } else if constexpr (std::is_same_v<T, Boolean>) {
                hash_combine(seed, value.value);
            } else if constexpr (std::is_same_v<T, String>) {
                hash_combine(seed, value.value);
            } else if constexpr (std::is_same_v<T, FunctionCall>) {
                hash_combine(seed, value.head);
                hash_combine(seed, structural_hash_list(value.args));
            } else if constexpr (std::is_same_v<T, FunctionDefinition>) {
                hash_combine(seed, value.name);
                hash_combine(seed, value.delayed);
                hash_combine(seed, value.params.size());
                for (const auto& param : value.params) {
                    hash_combine(seed, param.name);
                    hash_combine(seed, structural_hash(param.default_value));
                }
                hash_combine(seed, structural_hash(value.body));
            } else if constexpr (std::is_same_v<T, Assignment>) {
                hash_combine(seed, value.name);
                hash_combine(seed, structural_hash(value.value));
            } else if constexpr (std::is_same_v<T, Rule>) {
                hash_combine(seed, structural_hash(value.lhs));
                hash_combine(seed, structural_hash(value.rhs));
            } else if constexpr (std::is_same_v<T, List>) {
                hash_combine(seed, structural_hash_list(value.elements));
            } else if constexpr (std::is_same_v<T, Infinity> ||
                                 std::is_same_v<T, ComplexInfinity> ||
                                 std::is_same_v<T, Indeterminate>) {
            }
        },
        expr);

    return seed;
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

std::size_t structural_hash(const ExprPtr& expr) noexcept {
    if (expr == nullptr) {
        return 0;
    }
    return structural_hash_impl(*expr);
}

}  // namespace aleph3
