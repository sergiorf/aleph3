#include "kernel/ExactScalar.hpp"

#include <cctype>
#include <limits>
#include <stdexcept>

namespace aleph3::kernel {

namespace {

using boost::multiprecision::cpp_int;

cpp_int int64_min_value() {
    return cpp_int(std::numeric_limits<int64_t>::min());
}

cpp_int int64_max_value() {
    return cpp_int(std::numeric_limits<int64_t>::max());
}

cpp_int parse_decimal_cpp_int(std::string_view text) {
    if (text.empty()) {
        throw std::invalid_argument("Exact integer literal is empty");
    }

    std::size_t cursor = 0;
    bool negative = false;
    if (text[cursor] == '+' || text[cursor] == '-') {
        negative = text[cursor] == '-';
        ++cursor;
    }
    if (cursor == text.size()) {
        throw std::invalid_argument("Exact integer literal has no digits");
    }

    cpp_int value = 0;
    for (; cursor < text.size(); ++cursor) {
        const unsigned char ch = static_cast<unsigned char>(text[cursor]);
        if (!std::isdigit(ch)) {
            throw std::invalid_argument("Exact integer literal contains a non-digit");
        }
        value *= 10;
        value += text[cursor] - '0';
    }
    return negative ? -value : value;
}

}  // namespace

ExactInteger::ExactInteger() = default;

ExactInteger::ExactInteger(int value) : value_(value) {}

ExactInteger::ExactInteger(int64_t value) : value_(value) {}

ExactInteger::ExactInteger(cpp_int value) : value_(std::move(value)) {}

ExactInteger ExactInteger::from_decimal_string(std::string_view text) {
    return ExactInteger(parse_decimal_cpp_int(text));
}

bool ExactInteger::is_zero() const {
    return value_ == 0;
}

bool ExactInteger::is_one() const {
    return value_ == 1;
}

bool ExactInteger::is_negative() const {
    return value_ < 0;
}

int ExactInteger::sign() const {
    if (value_ < 0) return -1;
    if (value_ > 0) return 1;
    return 0;
}

std::optional<int64_t> ExactInteger::to_int64() const {
    if (value_ < int64_min_value() || value_ > int64_max_value()) {
        return std::nullopt;
    }
    return value_.convert_to<int64_t>();
}

std::string ExactInteger::to_string() const {
    return value_.convert_to<std::string>();
}

const cpp_int& ExactInteger::value() const noexcept {
    return value_;
}

ExactInteger operator-(const ExactInteger& value) {
    return ExactInteger(cpp_int(-value.value_));
}

ExactInteger operator+(const ExactInteger& left, const ExactInteger& right) {
    return ExactInteger(cpp_int(left.value_ + right.value_));
}

ExactInteger operator-(const ExactInteger& left, const ExactInteger& right) {
    return ExactInteger(cpp_int(left.value_ - right.value_));
}

ExactInteger operator*(const ExactInteger& left, const ExactInteger& right) {
    return ExactInteger(cpp_int(left.value_ * right.value_));
}

ExactInteger operator/(const ExactInteger& left, const ExactInteger& right) {
    if (right.is_zero()) {
        throw std::domain_error("Exact integer division by zero");
    }
    return ExactInteger(cpp_int(left.value_ / right.value_));
}

ExactInteger operator%(const ExactInteger& left, const ExactInteger& right) {
    if (right.is_zero()) {
        throw std::domain_error("Exact integer modulo by zero");
    }
    return ExactInteger(cpp_int(left.value_ % right.value_));
}

bool operator<(const ExactInteger& left, const ExactInteger& right) {
    return left.value_ < right.value_;
}

bool operator>(const ExactInteger& left, const ExactInteger& right) {
    return right < left;
}

bool operator<=(const ExactInteger& left, const ExactInteger& right) {
    return !(right < left);
}

bool operator>=(const ExactInteger& left, const ExactInteger& right) {
    return !(left < right);
}

ExactInteger abs(const ExactInteger& value) {
    return value.is_negative() ? -value : value;
}

ExactInteger gcd(ExactInteger left, ExactInteger right) {
    left = abs(left);
    right = abs(right);
    while (!right.is_zero()) {
        const ExactInteger remainder = left % right;
        left = right;
        right = remainder;
    }
    return left;
}

ExactRational::ExactRational() : numerator_(0), denominator_(1) {}

ExactRational::ExactRational(int64_t numerator, int64_t denominator)
    : ExactRational(ExactInteger(numerator), ExactInteger(denominator)) {}

ExactRational::ExactRational(ExactInteger numerator, ExactInteger denominator) {
    if (denominator.is_zero()) {
        throw std::domain_error("Exact rational denominator cannot be zero");
    }

    if (denominator.is_negative()) {
        numerator = -numerator;
        denominator = -denominator;
    }

    if (numerator.is_zero()) {
        numerator_ = ExactInteger(0);
        denominator_ = ExactInteger(1);
        return;
    }

    const ExactInteger divisor = gcd(abs(numerator), denominator);
    numerator_ = numerator / divisor;
    denominator_ = denominator / divisor;
}

ExactRational ExactRational::from_bounded(int64_t numerator, int64_t denominator) {
    return ExactRational(numerator, denominator);
}

const ExactInteger& ExactRational::numerator() const noexcept {
    return numerator_;
}

const ExactInteger& ExactRational::denominator() const noexcept {
    return denominator_;
}

bool ExactRational::is_zero() const {
    return numerator_.is_zero();
}

bool ExactRational::is_one() const {
    return numerator_ == denominator_;
}

int ExactRational::sign() const {
    return numerator_.sign();
}

std::optional<std::pair<int64_t, int64_t>> ExactRational::to_bounded() const {
    const auto numerator = numerator_.to_int64();
    const auto denominator = denominator_.to_int64();
    if (!numerator || !denominator) {
        return std::nullopt;
    }
    return std::pair<int64_t, int64_t>{*numerator, *denominator};
}

std::string ExactRational::to_string() const {
    if (denominator_.is_one()) {
        return numerator_.to_string();
    }
    return numerator_.to_string() + "/" + denominator_.to_string();
}

bool operator<(const ExactRational& left, const ExactRational& right) {
    return compare(left, right) < 0;
}

ExactRational operator-(const ExactRational& value) {
    return ExactRational(-value.numerator_, value.denominator_);
}

ExactRational operator+(const ExactRational& left, const ExactRational& right) {
    return ExactRational(
        left.numerator_ * right.denominator_ + right.numerator_ * left.denominator_,
        left.denominator_ * right.denominator_);
}

ExactRational operator-(const ExactRational& left, const ExactRational& right) {
    return ExactRational(
        left.numerator_ * right.denominator_ - right.numerator_ * left.denominator_,
        left.denominator_ * right.denominator_);
}

ExactRational operator*(const ExactRational& left, const ExactRational& right) {
    return ExactRational(left.numerator_ * right.numerator_, left.denominator_ * right.denominator_);
}

ExactRational operator/(const ExactRational& left, const ExactRational& right) {
    if (right.numerator_.is_zero()) {
        throw std::domain_error("Exact rational division by zero");
    }
    return ExactRational(left.numerator_ * right.denominator_, left.denominator_ * right.numerator_);
}

int compare(const ExactRational& left, const ExactRational& right) {
    const ExactInteger scaled_left = left.numerator() * right.denominator();
    const ExactInteger scaled_right = right.numerator() * left.denominator();
    if (scaled_left < scaled_right) return -1;
    if (scaled_right < scaled_left) return 1;
    return 0;
}

}  // namespace aleph3::kernel
