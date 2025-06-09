#include "poly/Polynomial.hpp"
#include <sstream>
#include <cmath>

namespace aleph3 {

    constexpr double EPSILON = 1e-10;

    Polynomial::Polynomial() : coefficients({ 0.0 }) {}

    Polynomial::Polynomial(const std::vector<double>& coeffs) : coefficients(coeffs) {
        normalize();
    }

    void Polynomial::normalize() {
        while (coefficients.size() > 1 && std::abs(coefficients.back()) < EPSILON) {
            coefficients.pop_back();
        }
    }

    Polynomial Polynomial::operator+(const Polynomial& other) const {
        size_t max_size = std::max(coefficients.size(), other.coefficients.size());
        std::vector<double> result(max_size, 0.0);

        for (size_t i = 0; i < max_size; ++i) {
            double a = (i < coefficients.size()) ? coefficients[i] : 0.0;
            double b = (i < other.coefficients.size()) ? other.coefficients[i] : 0.0;
            result[i] = a + b;
        }

        return Polynomial(result);
    }

    Polynomial Polynomial::operator-(const Polynomial& other) const {
        size_t max_size = std::max(coefficients.size(), other.coefficients.size());
        std::vector<double> result(max_size, 0.0);

        for (size_t i = 0; i < max_size; ++i) {
            double a = (i < coefficients.size()) ? coefficients[i] : 0.0;
            double b = (i < other.coefficients.size()) ? other.coefficients[i] : 0.0;
            result[i] = a - b;
        }

        return Polynomial(result);
    }

    Polynomial Polynomial::operator*(const Polynomial& other) const {
        std::vector<double> result(coefficients.size() + other.coefficients.size() - 1, 0.0);

        for (size_t i = 0; i < coefficients.size(); ++i) {
            for (size_t j = 0; j < other.coefficients.size(); ++j) {
                result[i + j] += coefficients[i] * other.coefficients[j];
            }
        }

        return Polynomial(result);
    }

    std::pair<Polynomial, Polynomial> Polynomial::divide(const Polynomial& divisor) const {
        if (divisor.is_zero()) {
            throw std::domain_error("Polynomial division by zero");
        }

        Polynomial dividend(*this);
        Polynomial quotient;
        std::vector<double> quot_coeffs(std::max<int>(0, dividend.degree() - divisor.degree() + 1), 0.0);

        while (dividend.degree() >= divisor.degree() && !dividend.is_zero()) {
            size_t deg_diff = dividend.degree() - divisor.degree();
            double lead_coeff_ratio = dividend.coefficients.back() / divisor.coefficients.back();
            std::vector<double> monomial_coeffs(deg_diff + 1, 0.0);
            monomial_coeffs.back() = lead_coeff_ratio;

            Polynomial monomial(monomial_coeffs);
            quot_coeffs[deg_diff] = lead_coeff_ratio;

            dividend = dividend - (divisor * monomial);
            dividend.normalize();
        }

        quotient = Polynomial(quot_coeffs);
        return { quotient, dividend };
    }

    Polynomial Polynomial::gcd(const Polynomial& a, const Polynomial& b) {
        Polynomial A = a, B = b;
        while (!B.is_zero()) {
            auto [_, r] = A.divide(B);
            A = B;
            B = r;
        }

        // Normalize leading coefficient to 1 if not zero
        if (!A.is_zero()) {
            double lead = A.coefficients.back();
            for (auto& c : A.coefficients) c /= lead;
        }

        return A;
    }

    bool Polynomial::is_zero() const {
        return coefficients.size() == 1 && std::abs(coefficients[0]) < EPSILON;
    }

    size_t Polynomial::degree() const {
        return coefficients.size() - 1;
    }

    std::string Polynomial::to_string() const {
        std::ostringstream oss;
        bool first = true;
        for (int i = degree(); i >= 0; --i) {
            double c = coefficients[i];
            if (std::abs(c) < EPSILON) continue;

            if (!first && c > 0) oss << " + ";
            else if (c < 0) oss << (first ? "-" : " - ");

            double abs_c = std::abs(c);
            if (i == 0 || abs_c != 1.0)
                oss << abs_c;

            if (i >= 1) {
                oss << "x";
                if (i > 1) oss << "^" << i;
            }

            first = false;
        }

        if (first) oss << "0";
        return oss.str();
    }

} // namespace aleph3
