#pragma once

#include "expr/Expr.hpp"
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <utility>

namespace aleph3 {

    inline uint64_t unsigned_abs_int64(int64_t value) noexcept {
        return value < 0
            ? static_cast<uint64_t>(-(value + 1)) + 1
            : static_cast<uint64_t>(value);
    }

    inline int64_t checked_int64_negate(int64_t value) {
        if (value == std::numeric_limits<int64_t>::min()) {
            throw std::overflow_error("Exact coefficient overflow");
        }
        return -value;
    }

    inline int64_t checked_int64_add(int64_t left, int64_t right) {
        if ((right > 0 && left > std::numeric_limits<int64_t>::max() - right) ||
            (right < 0 && left < std::numeric_limits<int64_t>::min() - right)) {
            throw std::overflow_error("Exact coefficient overflow");
        }
        return left + right;
    }

    inline int64_t checked_int64_subtract(int64_t left, int64_t right) {
        if (right == std::numeric_limits<int64_t>::min()) {
            if (left >= 0) {
                throw std::overflow_error("Exact coefficient overflow");
            }
            return left - right;
        }
        return checked_int64_add(left, -right);
    }

    inline int64_t checked_int64_multiply(int64_t left, int64_t right) {
        if (left == 0 || right == 0) {
            return 0;
        }
        if ((left == -1 && right == std::numeric_limits<int64_t>::min()) ||
            (right == -1 && left == std::numeric_limits<int64_t>::min())) {
            throw std::overflow_error("Exact coefficient overflow");
        }
        if (left > 0) {
            if ((right > 0 && left > std::numeric_limits<int64_t>::max() / right) ||
                (right < 0 && right < std::numeric_limits<int64_t>::min() / left)) {
                throw std::overflow_error("Exact coefficient overflow");
            }
        } else if ((right > 0 && left < std::numeric_limits<int64_t>::min() / right) ||
                   (right < 0 && left < std::numeric_limits<int64_t>::max() / right)) {
            throw std::overflow_error("Exact coefficient overflow");
        }
        return left * right;
    }

    inline int64_t divide_int64_by_unsigned(int64_t value, uint64_t divisor) {
        if (divisor == 0) {
            throw std::runtime_error("Denominator cannot be zero");
        }
        const uint64_t quotient_abs = unsigned_abs_int64(value) / divisor;
        if (value < 0) {
            if (quotient_abs == (uint64_t{1} << 63)) {
                return std::numeric_limits<int64_t>::min();
            }
            return -static_cast<int64_t>(quotient_abs);
        }
        if (quotient_abs > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
            throw std::overflow_error("Exact coefficient overflow");
        }
        return static_cast<int64_t>(quotient_abs);
    }

    inline std::pair<int64_t, int64_t> normalize_rational(int64_t num, int64_t den) {
        if (den == 0) throw std::runtime_error("Denominator cannot be zero");
        const uint64_t g = std::gcd(unsigned_abs_int64(num), unsigned_abs_int64(den));
        num = divide_int64_by_unsigned(num, g);
        den = divide_int64_by_unsigned(den, g);
        // Move sign to numerator, denominator always positive
        if (den < 0) {
            num = checked_int64_negate(num);
            den = checked_int64_negate(den);
        }
        return {num, den};
    }

    inline std::optional<int64_t> bounded_int64_from_integer(const Integer& value) {
        return value.value.to_int64();
    }

    inline std::optional<int64_t> bounded_int64_from_rational_integer(const Rational& value) {
        if (!value.denominator.is_one()) {
            return std::nullopt;
        }
        return value.numerator.to_int64();
    }

    inline std::optional<double> finite_double_from_exact_integer(const kernel::ExactInteger& value) {
        const double converted = value.value().convert_to<double>();
        if (!std::isfinite(converted)) {
            return std::nullopt;
        }
        return converted;
    }

    inline std::optional<double> finite_double_from_exact_rational(const Rational& value) {
        const auto numerator = finite_double_from_exact_integer(value.numerator);
        const auto denominator = finite_double_from_exact_integer(value.denominator);
        if (!numerator || !denominator || *denominator == 0.0) {
            return std::nullopt;
        }
        const double converted = *numerator / *denominator;
        if (!std::isfinite(converted)) {
            return std::nullopt;
        }
        return converted;
    }

    inline std::optional<double> finite_double_from_expr(const ExprPtr& expr) {
        if (const auto* number = std::get_if<Number>(expr.get())) {
            return number->value;
        }
        if (const auto* integer = std::get_if<Integer>(expr.get())) {
            return finite_double_from_exact_integer(integer->value);
        }
        if (const auto* rational = std::get_if<Rational>(expr.get())) {
            return finite_double_from_exact_rational(*rational);
        }
        return std::nullopt;
    }

    inline std::optional<int64_t> exact_int64_from_number(double value) {
        if (!std::isfinite(value) || std::floor(value) != value) {
            return std::nullopt;
        }
        constexpr double min_int64_as_double = -9223372036854775808.0;
        constexpr double past_max_int64_as_double = 9223372036854775808.0;
        if (value < min_int64_as_double || value >= past_max_int64_as_double) {
            return std::nullopt;
        }
        return static_cast<int64_t>(value);
    }

    inline std::optional<int64_t> exact_int64_from_expr(const ExprPtr& expr) {
        if (const auto* integer = std::get_if<Integer>(expr.get())) {
            return bounded_int64_from_integer(*integer);
        }
        if (const auto* rational = std::get_if<Rational>(expr.get())) {
            return bounded_int64_from_rational_integer(*rational);
        }
        if (const auto* number = std::get_if<Number>(expr.get())) {
            return exact_int64_from_number(number->value);
        }
        return std::nullopt;
    }

    inline std::pair<int64_t, int64_t> checked_rational_add(
        int64_t left_num,
        int64_t left_den,
        int64_t right_num,
        int64_t right_den) {
        const uint64_t common = std::gcd(unsigned_abs_int64(left_den), unsigned_abs_int64(right_den));
        const int64_t left_scale = divide_int64_by_unsigned(right_den, common);
        const int64_t right_scale = divide_int64_by_unsigned(left_den, common);
        return normalize_rational(
            checked_int64_add(
                checked_int64_multiply(left_num, left_scale),
                checked_int64_multiply(right_num, right_scale)),
            checked_int64_multiply(left_den, left_scale));
    }

    inline std::pair<int64_t, int64_t> checked_rational_subtract(
        int64_t left_num,
        int64_t left_den,
        int64_t right_num,
        int64_t right_den) {
        const uint64_t common = std::gcd(unsigned_abs_int64(left_den), unsigned_abs_int64(right_den));
        const int64_t left_scale = divide_int64_by_unsigned(right_den, common);
        const int64_t right_scale = divide_int64_by_unsigned(left_den, common);
        return normalize_rational(
            checked_int64_subtract(
                checked_int64_multiply(left_num, left_scale),
                checked_int64_multiply(right_num, right_scale)),
            checked_int64_multiply(left_den, left_scale));
    }

    inline std::pair<int64_t, int64_t> checked_rational_multiply(
        int64_t left_num,
        int64_t left_den,
        int64_t right_num,
        int64_t right_den) {
        const uint64_t left_cancel = std::gcd(unsigned_abs_int64(left_num), unsigned_abs_int64(right_den));
        const uint64_t right_cancel = std::gcd(unsigned_abs_int64(right_num), unsigned_abs_int64(left_den));
        return normalize_rational(
            checked_int64_multiply(
                divide_int64_by_unsigned(left_num, left_cancel),
                divide_int64_by_unsigned(right_num, right_cancel)),
            checked_int64_multiply(
                divide_int64_by_unsigned(left_den, right_cancel),
                divide_int64_by_unsigned(right_den, left_cancel)));
    }

    inline std::pair<int64_t, int64_t> checked_rational_divide(
        int64_t left_num,
        int64_t left_den,
        int64_t right_num,
        int64_t right_den) {
        if (right_num == 0) {
            throw std::runtime_error("Denominator cannot be zero");
        }
        const uint64_t numerator_cancel = std::gcd(unsigned_abs_int64(left_num), unsigned_abs_int64(right_num));
        const uint64_t denominator_cancel = std::gcd(unsigned_abs_int64(left_den), unsigned_abs_int64(right_den));
        return normalize_rational(
            checked_int64_multiply(
                divide_int64_by_unsigned(left_num, numerator_cancel),
                divide_int64_by_unsigned(right_den, denominator_cancel)),
            checked_int64_multiply(
                divide_int64_by_unsigned(left_den, denominator_cancel),
                divide_int64_by_unsigned(right_num, numerator_cancel)));
    }

    inline int checked_rational_compare(
        int64_t left_num,
        int64_t left_den,
        int64_t right_num,
        int64_t right_den) {
        const uint64_t common = std::gcd(unsigned_abs_int64(left_den), unsigned_abs_int64(right_den));
        const int64_t left_scale = divide_int64_by_unsigned(right_den, common);
        const int64_t right_scale = divide_int64_by_unsigned(left_den, common);
        const int64_t left_scaled = checked_int64_multiply(left_num, left_scale);
        const int64_t right_scaled = checked_int64_multiply(right_num, right_scale);
        if (left_scaled < right_scaled) return -1;
        if (left_scaled > right_scaled) return 1;
        return 0;
    }

    inline std::pair<kernel::ExactInteger, kernel::ExactInteger> normalize_rational(
        kernel::ExactInteger num,
        kernel::ExactInteger den) {
        const kernel::ExactRational rational(std::move(num), std::move(den));
        return {rational.numerator(), rational.denominator()};
    }

    inline std::pair<kernel::ExactInteger, kernel::ExactInteger> checked_rational_add(
        const kernel::ExactInteger& left_num,
        const kernel::ExactInteger& left_den,
        const kernel::ExactInteger& right_num,
        const kernel::ExactInteger& right_den) {
        const kernel::ExactRational result(
            kernel::ExactRational(left_num, left_den) +
            kernel::ExactRational(right_num, right_den));
        return {result.numerator(), result.denominator()};
    }

    inline std::pair<kernel::ExactInteger, kernel::ExactInteger> checked_rational_subtract(
        const kernel::ExactInteger& left_num,
        const kernel::ExactInteger& left_den,
        const kernel::ExactInteger& right_num,
        const kernel::ExactInteger& right_den) {
        const kernel::ExactRational result(
            kernel::ExactRational(left_num, left_den) -
            kernel::ExactRational(right_num, right_den));
        return {result.numerator(), result.denominator()};
    }

    inline std::pair<kernel::ExactInteger, kernel::ExactInteger> checked_rational_multiply(
        const kernel::ExactInteger& left_num,
        const kernel::ExactInteger& left_den,
        const kernel::ExactInteger& right_num,
        const kernel::ExactInteger& right_den) {
        const kernel::ExactRational result(
            kernel::ExactRational(left_num, left_den) *
            kernel::ExactRational(right_num, right_den));
        return {result.numerator(), result.denominator()};
    }

    inline std::pair<kernel::ExactInteger, kernel::ExactInteger> checked_rational_divide(
        const kernel::ExactInteger& left_num,
        const kernel::ExactInteger& left_den,
        const kernel::ExactInteger& right_num,
        const kernel::ExactInteger& right_den) {
        const kernel::ExactRational result(
            kernel::ExactRational(left_num, left_den) /
            kernel::ExactRational(right_num, right_den));
        return {result.numerator(), result.denominator()};
    }

    inline int checked_rational_compare(
        const kernel::ExactInteger& left_num,
        const kernel::ExactInteger& left_den,
        const kernel::ExactInteger& right_num,
        const kernel::ExactInteger& right_den) {
        return kernel::compare(
            kernel::ExactRational(left_num, left_den),
            kernel::ExactRational(right_num, right_den));
    }

    inline double get_number_value(const ExprPtr& expr) {
        if (auto num = std::get_if<Number>(&(*expr))) {
            return num->value;
        }
        if (auto integer = std::get_if<Integer>(&(*expr))) {
            if (auto value = finite_double_from_exact_integer(integer->value)) {
                return *value;
            }
        }
        if (auto rational = std::get_if<Rational>(&(*expr))) {
            if (auto value = finite_double_from_exact_rational(*rational)) {
                return *value;
            }
        }
        throw std::runtime_error("Expected a finite numeric atom during evaluation, but got something else");
    }

    inline bool get_boolean_value(const ExprPtr& expr) {
        if (std::holds_alternative<Boolean>(*expr)) {
            return std::get<Boolean>(*expr).value;
        }
        throw std::runtime_error("Expression is not a Boolean");
    }

    inline bool is_zero(const ExprPtr& e) {
        if (const auto* number = std::get_if<Number>(e.get())) {
            return number->value == 0.0;
        }
        if (const auto* integer = std::get_if<Integer>(e.get())) {
            return integer->value.is_zero();
        }
        if (const auto* rational = std::get_if<Rational>(e.get())) {
            return rational->numerator.is_zero();
        }
        return false;
    }

    inline bool is_one(const ExprPtr& e) {
        if (const auto* number = std::get_if<Number>(e.get())) {
            return number->value == 1.0;
        }
        if (const auto* integer = std::get_if<Integer>(e.get())) {
            return integer->value.is_one();
        }
        if (const auto* rational = std::get_if<Rational>(e.get())) {
            return rational->numerator == rational->denominator;
        }
        return false;
    }

    inline bool is_function(const ExprPtr& e, const std::string& name) {
        auto f = std::get_if<FunctionCall>(e.get());
        return f && f->head == name;
    }

    inline ExprPtr make_number(double value) {
        return make_expr<Number>(value);
    }

    inline ExprPtr make_integer(kernel::ExactInteger value) {
        return make_expr<Integer>(std::move(value));
    }

    inline ExprPtr make_integer(int64_t value) {
        return make_expr<Integer>(value);
    }

    inline ExprPtr make_rational_expr(kernel::ExactInteger numerator, kernel::ExactInteger denominator) {
        return make_expr<Rational>(std::move(numerator), std::move(denominator));
    }

    inline ExprPtr make_rational_expr(int64_t numerator, int64_t denominator) {
        return make_expr<Rational>(numerator, denominator);
    }

    inline ExprPtr make_exact_scalar_expr(const kernel::ExactRational& value) {
        if (value.denominator().is_one()) {
            return make_integer(value.numerator());
        }
        return make_expr<Rational>(value);
    }

    inline ExprPtr make_plus(const ExprPtr& a, const ExprPtr& b) {
        return make_expr<FunctionCall>("Plus", std::vector<ExprPtr>{a, b});
    }

    inline ExprPtr make_plus(std::initializer_list<ExprPtr> args) {
        return make_expr<FunctionCall>("Plus", std::vector<ExprPtr>(args));
    }

    inline ExprPtr make_plus(const std::vector<ExprPtr>& args) {
        return make_expr<FunctionCall>("Plus", args);
    }

    inline ExprPtr make_times(const std::vector<ExprPtr>& args) {
        std::vector<ExprPtr> flattened;
        double coefficient = 1.0;

        for (const auto& arg : args) {
            if (is_one(arg)) continue;

            if (auto num = std::get_if<Number>(arg.get())) {
                coefficient *= num->value;
            }
            else if (is_function(arg, "Times")) {
                const auto& inner = std::get<FunctionCall>(*arg);
                for (const auto& inner_arg : inner.args) {
                    flattened.push_back(inner_arg);
                }
            }
            else {
                flattened.push_back(arg);
            }
        }

        if (coefficient != 1.0) {
            flattened.insert(flattened.begin(), make_number(coefficient));
        }

        if (flattened.empty()) {
            return make_number(coefficient); // possibly 1 or 0
        }

        if (flattened.size() == 1) {
            return flattened[0]; // no need for Times head
        }

        return make_expr<FunctionCall>("Times", flattened);
    }

    inline ExprPtr make_times(const ExprPtr& a, const ExprPtr& b) {
        return make_times(std::vector<ExprPtr>{ a, b });
    }

    inline ExprPtr make_times(std::initializer_list<ExprPtr> args) {
        return make_times(std::vector<ExprPtr>(args));
    }

    inline ExprPtr make_pow(const ExprPtr& base, int exponent) {
        return make_expr<FunctionCall>("Power", std::vector<ExprPtr>{
            base, make_expr<Integer>(exponent)
        });
    }

    inline ExprPtr make_number(int value) {
        return make_expr<Number>(static_cast<double>(value));
    }

    inline int get_integer_value(const ExprPtr& e) {
        if (auto n = exact_int64_from_expr(e)) {
            if (*n >= std::numeric_limits<int>::min() &&
                *n <= std::numeric_limits<int>::max()) {
                return static_cast<int>(*n);
            }
        }
        throw std::runtime_error("Expected integer number");
    }

    inline ExprPtr make_fcall(std::string name, const std::vector<ExprPtr>& args) {
        return make_expr<FunctionCall>(name, args);
    }

    inline ExprPtr make_fcall(std::string name, std::initializer_list<ExprPtr> args) {
        return make_expr<FunctionCall>(name, std::vector<ExprPtr>(args));
    }

    inline ExprPtr make_fdef(std::string name, std::initializer_list<Parameter> params, const ExprPtr& body, bool delayed) {
        return make_expr<FunctionDefinition>(name, std::vector<Parameter>(params), body, delayed);
    }

    inline ExprPtr make_fdef(std::string name, std::initializer_list<std::string> args, const ExprPtr& body, bool delayed) {
        std::vector<Parameter> params;
        for (const auto& arg : args) {
            params.emplace_back(arg);
        }
        return make_expr<FunctionDefinition>(name, params, body, delayed);
    }

    inline ExprPtr make_expr(const Indeterminate&) {
        return std::make_shared<Expr>(Indeterminate{});
    }
}
