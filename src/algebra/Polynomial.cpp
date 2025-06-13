#include "algebra/Polynomial.hpp"
#include <sstream>
#include <cmath>
#include <algorithm>

namespace aleph3 {

    constexpr double EPSILON = 1e-10;

    // Default constructor: zero polynomial
    Polynomial::Polynomial() : terms{} {}

    // Construct from map of monomials to coefficients
    Polynomial::Polynomial(const std::map<Monomial, double>& t) : terms(t) {
        normalize();
    }

    // Construct from a single constant
    Polynomial::Polynomial(double constant) {
        if (std::abs(constant) >= EPSILON)
            terms[Monomial{}] = constant; // Monomial{} is the empty monomial (degree 0)
    }

    // Remove zero coefficients
    void Polynomial::normalize() {
        for (auto it = terms.begin(); it != terms.end(); ) {
            if (std::abs(it->second) < EPSILON)
                it = terms.erase(it);
            else
                ++it;
        }
        if (terms.empty())
            terms[Monomial{}] = 0.0;
    }

    // Addition
    Polynomial Polynomial::operator+(const Polynomial& other) const {
        Polynomial result = *this;
        for (const auto& [mono, coeff] : other.terms) {
            result.terms[mono] += coeff;
        }
        result.normalize();
        return result;
    }

    // Subtraction
    Polynomial Polynomial::operator-(const Polynomial& other) const {
        Polynomial result = *this;
        for (const auto& [mono, coeff] : other.terms) {
            result.terms[mono] -= coeff;
        }
        result.normalize();
        return result;
    }

    // Multiplication
    Polynomial Polynomial::operator*(const Polynomial& other) const {
        Polynomial result;
        for (const auto& [m1, c1] : terms) {
            for (const auto& [m2, c2] : other.terms) {
                Monomial m = m1;
                for (const auto& [var, exp] : m2) {
                    m[var] += exp;
                }
                result.terms[m] += c1 * c2;
            }
        }
        result.normalize();
        return result;
    }

    // Division (univariate only for now)
    std::pair<Polynomial, Polynomial> Polynomial::divide(const Polynomial& divisor) const {
        // Only supports univariate division for now
        // Find the variable name (if any)
        std::string var;
        for (const auto& [mono, _] : divisor.terms) {
            for (const auto& [v, _e] : mono) {
                var = v;
                break;
            }
            if (!var.empty()) break;
        }
        if (var.empty()) {
            // Divisor is a constant
            double c = 0.0;
            for (const auto& [mono, coeff] : divisor.terms) {
                if (mono.empty()) {
                    c = coeff;
                    break;
                }
            }
            if (std::abs(c) < 1e-10)
                throw std::domain_error("Polynomial division by zero");
            Polynomial quotient;
            for (const auto& [mono, coeff] : this->terms) {
                quotient.terms[mono] = coeff / c;
            }
            quotient.normalize();
            Polynomial remainder(0.0);
            return { quotient, remainder };
        }
        Polynomial remainder = *this;
        Polynomial quotient;

        auto deg = [&](const Polynomial& p) -> int {
            int d = -1;
            for (const auto& [mono, coeff] : p.terms) {
                if (std::abs(coeff) < EPSILON) continue;
                int sum = 0;
                auto it = mono.find(var);
                if (it != mono.end()) sum = it->second;
                d = std::max(d, sum);
            }
            return d;
            };

        int divisor_deg = deg(divisor);
        if (divisor_deg < 0)
            throw std::domain_error("Division by zero polynomial");

        while (deg(remainder) >= divisor_deg && !remainder.is_zero()) {
            // Find leading term of remainder and divisor
            Monomial lead_mono_r;
            double lead_coeff_r = 0.0;
            int max_deg_r = -1;
            for (const auto& [mono, coeff] : remainder.terms) {
                int d = 0;
                auto it = mono.find(var);
                if (it != mono.end()) d = it->second;
                if (d > max_deg_r && std::abs(coeff) >= EPSILON) {
                    max_deg_r = d;
                    lead_mono_r = mono;
                    lead_coeff_r = coeff;
                }
            }
            Monomial lead_mono_d;
            double lead_coeff_d = 0.0;
            int max_deg_d = -1;
            for (const auto& [mono, coeff] : divisor.terms) {
                int d = 0;
                auto it = mono.find(var);
                if (it != mono.end()) d = it->second;
                if (d > max_deg_d && std::abs(coeff) >= EPSILON) {
                    max_deg_d = d;
                    lead_mono_d = mono;
                    lead_coeff_d = coeff;
                }
            }
            if (max_deg_r < max_deg_d) break;

            // Compute monomial for quotient
            Monomial qmono = lead_mono_r;
            for (const auto& [v, e] : lead_mono_d) {
                qmono[v] -= e;
                if (qmono[v] == 0) qmono.erase(v);
            }
            double qcoeff = lead_coeff_r / lead_coeff_d;
            Polynomial qterm({ {qmono, qcoeff} });
            quotient = quotient + qterm;
            remainder = remainder - (divisor * qterm);
            remainder.normalize();
        }

        quotient.normalize();
        remainder.normalize();
        return { quotient, remainder };
    }

    // GCD (univariate only for now)
    Polynomial Polynomial::gcd(const Polynomial& a, const Polynomial& b, const std::string& var) {
        Polynomial A = a, B = b;
        while (!B.is_zero()) {
            auto [_, r] = A.divide(B);
            A = B;
            B = r;
        }
        // Normalize leading coefficient to 1 if not zero
        if (!A.is_zero()) {
            // Find leading coefficient
            double lead = 0.0;
            int max_deg = -1;
            for (const auto& [mono, coeff] : A.terms) {
                int d = 0;
                auto it = mono.find(var);
                if (it != mono.end()) d = it->second;
                if (d > max_deg && std::abs(coeff) >= EPSILON) {
                    max_deg = d;
                    lead = coeff;
                }
            }
            if (std::abs(lead) >= EPSILON) {
                for (auto& [mono, coeff] : A.terms) coeff /= lead;
            }
        }
        return A;
    }

    bool Polynomial::is_zero() const {
        return terms.size() == 1 && terms.begin()->second == 0.0 && terms.begin()->first.empty();
    }

    size_t Polynomial::degree() const {
        size_t max_deg = 0;
        for (const auto& [mono, coeff] : terms) {
            size_t deg = 0;
            for (const auto& [_, e] : mono) deg += e;
            if (std::abs(coeff) >= EPSILON)
                max_deg = std::max(max_deg, deg);
        }
        return max_deg;
    }

    std::string Polynomial::to_string() const {
        std::ostringstream oss;
        bool first = true;
        for (const auto& [mono, coeff] : terms) {
            if (std::abs(coeff) < EPSILON) continue;
            if (!first) oss << (coeff >= 0 ? " + " : " - ");
            else if (coeff < 0) oss << "-";
            double abs_c = std::abs(coeff);
            if (abs_c != 1.0 || mono.empty())
                oss << abs_c;
            for (const auto& [var, exp] : mono) {
                oss << "*" << var;
                if (exp > 1) oss << "^" << exp;
            }
            first = false;
        }
        if (first) oss << "0";
        return oss.str();
    }

} // namespace aleph3