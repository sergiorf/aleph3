#pragma once
#include "algebra/Polynomial.hpp"
#include <map>
#include <string>
#include <vector>

namespace aleph3::test {

// Construct a multivariate polynomial from a list of (coeff, {var,exp}...) tuples
inline Polynomial make_poly(const std::vector<std::pair<double, std::map<std::string, int>>>& terms) {
    std::map<Monomial, double> poly_terms;
    for (const auto& [coeff, exps] : terms) {
        Monomial m;
        for (const auto& [var, exp] : exps) {
            if (exp != 0) m[var] = exp;
        }
        poly_terms[m] += coeff;
    }
    return Polynomial(poly_terms);
}

// Get coefficient for a specific monomial
inline double get_coeff(const Polynomial& poly, const std::map<std::string, int>& exps) {
    Monomial m;
    for (const auto& [var, exp] : exps) {
        if (exp != 0) m[var] = exp;
    }
    auto it = poly.terms.find(m);
    return (it != poly.terms.end()) ? it->second : 0.0;
}

} // namespace aleph3::test