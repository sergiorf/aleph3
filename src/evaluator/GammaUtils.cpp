#include "evaluator/GammaUtils.hpp"

#include "Constants.hpp"
#include "expr/ExprUtils.hpp"
#include "normalizer/Normalizer.hpp"

#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace aleph3 {

namespace {

constexpr double SQRT_TWO_PI = 2.50662827463100050242;
constexpr int64_t MAX_SYMBOLIC_GAMMA_SHIFT = 64;

bool is_integer_like(double value, double tolerance = 1e-12) {
    return std::abs(value - std::round(value)) < tolerance;
}

bool is_nonpositive_integer(double value) {
    return value <= 0.0 && is_integer_like(value);
}

std::complex<double> complex_gamma_lanczos(std::complex<double> z) {
    static const double coefficients[] = {
        0.99999999999980993,
        676.5203681218851,
        -1259.1392167224028,
        771.32342877765313,
        -176.61502916214059,
        12.507343278686905,
        -0.13857109526572012,
        9.9843695780195716e-6,
        1.5056327351493116e-7
    };

    if (std::real(z) < 0.5) {
        const auto pi_z = std::complex<double>(PI, 0.0) * z;
        return std::complex<double>(PI, 0.0) /
               (std::sin(pi_z) * complex_gamma_lanczos(std::complex<double>(1.0, 0.0) - z));
    }

    z -= std::complex<double>(1.0, 0.0);
    std::complex<double> x(coefficients[0], 0.0);
    for (size_t i = 1; i < std::size(coefficients); ++i) {
        x += coefficients[i] / (z + static_cast<double>(i));
    }

    const std::complex<double> t = z + 7.5;
    return SQRT_TWO_PI * std::pow(t, z + 0.5) * std::exp(-t) * x;
}

ExprPtr make_exact_gamma_half_integer(int odd_numerator) {
    int64_t coeff_num = 1;
    int64_t coeff_den = 1;
    double current = 0.5;
    const double target = static_cast<double>(odd_numerator) / 2.0;

    if (target > current) {
        while (current < target) {
            auto [nn, dd] = normalize_rational(
                coeff_num * static_cast<int64_t>(current * 2.0),
                coeff_den * 2);
            coeff_num = nn;
            coeff_den = dd;
            current += 1.0;
        }
    } else if (target < current) {
        while (current > target) {
            const double next = current - 1.0;
            auto [nn, dd] = normalize_rational(
                coeff_num * 2,
                coeff_den * static_cast<int64_t>(next * 2.0));
            coeff_num = nn;
            coeff_den = dd;
            current = next;
        }
    }

    ExprPtr sqrt_pi = make_fcall("Sqrt", {make_expr<Symbol>("Pi")});
    if (coeff_num == 1 && coeff_den == 1) {
        return sqrt_pi;
    }
    return normalize_expr(make_fcall("Times", {make_expr<Rational>(coeff_num, coeff_den), sqrt_pi}));
}

ExprPtr make_gamma_shifted_term(const ExprPtr& base, int64_t offset) {
    if (offset == 0) {
        return base;
    }

    std::vector<ExprPtr> plus_args;
    plus_args.push_back(make_integer(offset));

    if (const auto* base_plus = std::get_if<FunctionCall>(base.get());
        base_plus != nullptr && base_plus->head == "Plus") {
        plus_args.insert(plus_args.end(), base_plus->args.begin(), base_plus->args.end());
    } else {
        plus_args.push_back(base);
    }

    return normalize_expr(make_fcall("Plus", plus_args));
}

std::optional<std::pair<ExprPtr, int64_t>> extract_integer_shift_base(const ExprPtr& arg) {
    const auto* plus = std::get_if<FunctionCall>(arg.get());
    if (plus == nullptr || plus->head != "Plus") {
        return std::nullopt;
    }

    int64_t integer_shift = 0;
    bool has_integer_shift = false;
    std::vector<ExprPtr> base_terms;
    base_terms.reserve(plus->args.size());

    for (const auto& term : plus->args) {
        if (auto integer = exact_int64_from_expr(term)) {
            integer_shift += *integer;
            has_integer_shift = true;
            continue;
        }

        if (const auto* number = std::get_if<Number>(term.get());
            number != nullptr &&
            std::isfinite(number->value) &&
            is_integer_like(number->value) &&
            std::abs(number->value) <= static_cast<double>(std::numeric_limits<int64_t>::max())) {
            integer_shift += static_cast<int64_t>(std::llround(number->value));
            has_integer_shift = true;
            continue;
        }

        if (const auto* rational = std::get_if<Rational>(term.get());
            rational != nullptr) {
            const auto bounded = rational->exact().to_bounded();
            if (!bounded.has_value()) {
                base_terms.push_back(term);
                continue;
            }
            if (bounded->second == 1) {
                integer_shift += bounded->first;
                has_integer_shift = true;
                continue;
            }

            const int64_t truncated_shift = bounded->first / bounded->second;
            const int64_t remainder_num = bounded->first % bounded->second;
            if (truncated_shift != 0) {
                integer_shift += truncated_shift;
                has_integer_shift = true;
            }
            if (remainder_num != 0) {
                base_terms.push_back(make_expr<Rational>(remainder_num, bounded->second));
            }
            continue;
        }

        base_terms.push_back(term);
    }

    if (!has_integer_shift || integer_shift == 0 || base_terms.empty() ||
        std::abs(integer_shift) > MAX_SYMBOLIC_GAMMA_SHIFT) {
        return std::nullopt;
    }

    ExprPtr base =
        base_terms.size() == 1 ? base_terms.front() : normalize_expr(make_fcall("Plus", base_terms));
    return std::make_pair(base, integer_shift);
}

ExprPtr make_gamma_recurrence(const ExprPtr& base, int64_t integer_shift) {
    if (integer_shift > 0) {
        std::vector<ExprPtr> factors;
        factors.reserve(static_cast<size_t>(integer_shift) + 1);
        for (int64_t i = 0; i < integer_shift; ++i) {
            factors.push_back(make_gamma_shifted_term(base, i));
        }
        factors.push_back(make_fcall("Gamma", {base}));
        return normalize_expr(make_fcall("Times", factors));
    }

    std::vector<ExprPtr> denominator_factors;
    denominator_factors.reserve(static_cast<size_t>(-integer_shift));
    for (int64_t i = 1; i <= -integer_shift; ++i) {
        denominator_factors.push_back(make_gamma_shifted_term(base, -i));
    }

    ExprPtr denominator = denominator_factors.size() == 1
        ? denominator_factors.front()
        : normalize_expr(make_fcall("Times", denominator_factors));

    return normalize_expr(make_fcall("Divide", {make_fcall("Gamma", {base}), denominator}));
}

}  // namespace

std::optional<ExprPtr> simplify_gamma_argument(const ExprPtr& arg) {
    if (std::holds_alternative<Number>(*arg)) {
        const double value = get_number_value(arg);
        if (is_nonpositive_integer(value)) {
            return make_expr<ComplexInfinity>();
        }
        return make_expr<Number>(std::tgamma(value));
    }

    if (std::holds_alternative<Integer>(*arg)) {
        const auto& integer = std::get<Integer>(*arg);
        if (integer.value <= 0) {
            return make_expr<ComplexInfinity>();
        }
        const auto bounded = integer.value.to_int64();
        if (!bounded.has_value()) {
            return std::nullopt;
        }
        return make_expr<Number>(std::tgamma(static_cast<double>(*bounded)));
    }

    if (std::holds_alternative<Rational>(*arg)) {
        const auto& rational = std::get<Rational>(*arg);
        const auto bounded = rational.exact().to_bounded();
        if (!bounded.has_value()) {
            return std::nullopt;
        }
        if (bounded->second == 1) {
            if (bounded->first <= 0) {
                return make_expr<ComplexInfinity>();
            }
            return make_expr<Number>(std::tgamma(static_cast<double>(bounded->first)));
        }
        if (bounded->second == 2 && (bounded->first % 2 != 0)) {
            return make_exact_gamma_half_integer(static_cast<int>(bounded->first));
        }

        const double value =
            static_cast<double>(bounded->first) / static_cast<double>(bounded->second);
        return make_expr<Number>(std::tgamma(value));
    }

    if (std::holds_alternative<Complex>(*arg)) {
        const auto& complex = std::get<Complex>(*arg);
        if (std::abs(complex.imag) < 1e-12 && is_nonpositive_integer(complex.real)) {
            return make_expr<ComplexInfinity>();
        }

        const auto value = complex_gamma_lanczos(std::complex<double>(complex.real, complex.imag));
        return make_expr<Complex>(value.real(), value.imag());
    }

    if (auto shifted = extract_integer_shift_base(arg)) {
        return make_gamma_recurrence(shifted->first, shifted->second);
    }

    return std::nullopt;
}

}  // namespace aleph3
