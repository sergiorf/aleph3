#pragma once

#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

namespace aleph3::syntax {

[[nodiscard]] inline std::optional<int64_t> parse_bounded_int64_decimal(
    std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }

    std::size_t cursor = 0;
    const bool negative = text.front() == '-';
    if (negative || text.front() == '+') {
        cursor = 1;
    }
    if (cursor == text.size()) {
        return std::nullopt;
    }

    const uint64_t limit = negative
        ? uint64_t{1} << 63
        : static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    uint64_t value = 0;
    for (; cursor < text.size(); ++cursor) {
        const unsigned char ch = static_cast<unsigned char>(text[cursor]);
        if (!std::isdigit(ch)) {
            return std::nullopt;
        }
        const uint64_t digit = static_cast<uint64_t>(text[cursor] - '0');
        if (value > (limit - digit) / 10) {
            return std::nullopt;
        }
        value = value * 10 + digit;
    }

    if (negative) {
        if (value == (uint64_t{1} << 63)) {
            return std::numeric_limits<int64_t>::min();
        }
        return -static_cast<int64_t>(value);
    }
    return static_cast<int64_t>(value);
}

[[nodiscard]] inline std::optional<double> exact_double_from_bounded_integer(
    int64_t value) {
    const double as_double = static_cast<double>(value);
    constexpr double min_int64_as_double = -9223372036854775808.0;
    constexpr double past_max_int64_as_double = 9223372036854775808.0;
    if (!std::isfinite(as_double) || as_double < min_int64_as_double ||
        as_double >= past_max_int64_as_double) {
        return std::nullopt;
    }

    if (static_cast<int64_t>(as_double) != value) {
        return std::nullopt;
    }
    return as_double;
}

}  // namespace aleph3::syntax
