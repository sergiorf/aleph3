/*
 * ExactScalar.hpp
 * ---------------
 * Kernel-owned arbitrary-precision integer and rational scalar values.
 *
 * Public Expr integer and rational atoms use these exact scalar values. Some
 * evaluator, SDK, and algebra adapters still apply explicit bounded gates.
 */

#pragma once

#include <boost/multiprecision/cpp_int.hpp>

#include <cstdint>
#include <compare>
#include <cstddef>
#include <functional>
#include <ostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace aleph3::kernel {

class ExactInteger {
public:
    ExactInteger();
    ExactInteger(int value);
    ExactInteger(int64_t value);
    explicit ExactInteger(boost::multiprecision::cpp_int value);

    [[nodiscard]] static ExactInteger from_decimal_string(std::string_view text);

    [[nodiscard]] bool is_zero() const;
    [[nodiscard]] bool is_one() const;
    [[nodiscard]] bool is_negative() const;
    [[nodiscard]] int sign() const;

    [[nodiscard]] std::optional<int64_t> to_int64() const;
    [[nodiscard]] std::string to_string() const;

    [[nodiscard]] const boost::multiprecision::cpp_int& value() const noexcept;

    friend bool operator==(const ExactInteger& left, const ExactInteger& right) = default;
    friend bool operator<(const ExactInteger& left, const ExactInteger& right);
    friend bool operator>(const ExactInteger& left, const ExactInteger& right);
    friend bool operator<=(const ExactInteger& left, const ExactInteger& right);
    friend bool operator>=(const ExactInteger& left, const ExactInteger& right);

    friend ExactInteger operator-(const ExactInteger& value);
    friend ExactInteger operator+(const ExactInteger& left, const ExactInteger& right);
    friend ExactInteger operator-(const ExactInteger& left, const ExactInteger& right);
    friend ExactInteger operator*(const ExactInteger& left, const ExactInteger& right);
    friend ExactInteger operator/(const ExactInteger& left, const ExactInteger& right);
    friend ExactInteger operator%(const ExactInteger& left, const ExactInteger& right);

private:
    boost::multiprecision::cpp_int value_;
};

[[nodiscard]] ExactInteger abs(const ExactInteger& value);
[[nodiscard]] ExactInteger gcd(ExactInteger left, ExactInteger right);

class ExactRational {
public:
    ExactRational();
    ExactRational(int64_t numerator, int64_t denominator);
    ExactRational(ExactInteger numerator, ExactInteger denominator);

    [[nodiscard]] static ExactRational from_bounded(int64_t numerator, int64_t denominator);

    [[nodiscard]] const ExactInteger& numerator() const noexcept;
    [[nodiscard]] const ExactInteger& denominator() const noexcept;

    [[nodiscard]] bool is_zero() const;
    [[nodiscard]] bool is_one() const;
    [[nodiscard]] int sign() const;

    [[nodiscard]] std::optional<std::pair<int64_t, int64_t>> to_bounded() const;
    [[nodiscard]] std::string to_string() const;

    friend bool operator==(const ExactRational& left, const ExactRational& right) = default;
    friend bool operator<(const ExactRational& left, const ExactRational& right);

    friend ExactRational operator-(const ExactRational& value);
    friend ExactRational operator+(const ExactRational& left, const ExactRational& right);
    friend ExactRational operator-(const ExactRational& left, const ExactRational& right);
    friend ExactRational operator*(const ExactRational& left, const ExactRational& right);
    friend ExactRational operator/(const ExactRational& left, const ExactRational& right);

private:
    ExactInteger numerator_;
    ExactInteger denominator_;
};

[[nodiscard]] int compare(const ExactRational& left, const ExactRational& right);

inline std::ostream& operator<<(std::ostream& out, const ExactInteger& value) {
    return out << value.to_string();
}

}  // namespace aleph3::kernel

namespace std {

template <>
struct hash<aleph3::kernel::ExactInteger> {
    std::size_t operator()(const aleph3::kernel::ExactInteger& value) const noexcept {
        return std::hash<std::string>{}(value.to_string());
    }
};

}  // namespace std
