/* Algebra-layer value and algorithm tests for exact dense matrices. */
#include "algebra/DenseMatrix.hpp"
#include "algebra/ExactPolynomial.hpp"

#include <catch2/catch_test_macros.hpp>

#include <limits>

using aleph3::ExactCoefficient;
using aleph3::algebra::DenseMatrix;

TEST_CASE("DenseMatrix owns checked row-major value storage", "[algebra][matrix]") {
    DenseMatrix<ExactCoefficient> matrix(2, 2, {{1, 1}, {2, 1}, {3, 1}, {4, 1}});
    REQUIRE(matrix.rows() == 2);
    REQUIRE(matrix.columns() == 2);
    REQUIRE(matrix(1, 0) == ExactCoefficient(3, 1));
    REQUIRE(matrix.values()[3] == ExactCoefficient(4, 1));
    REQUIRE_THROWS_AS(DenseMatrix<ExactCoefficient>(2, 2, {{1, 1}}), std::invalid_argument);
}

TEST_CASE("Dense matrix algorithms preserve exact rational arithmetic", "[algebra][matrix][exact]") {
    DenseMatrix<ExactCoefficient> left(2, 2, {{1, 2}, {1, 1}, {0, 1}, {2, 1}});
    DenseMatrix<ExactCoefficient> right(2, 2, {{2, 1}, {0, 1}, {1, 1}, {3, 1}});
    const auto product = aleph3::algebra::matrix_multiply(left, right);
    REQUIRE(product(0, 0) == ExactCoefficient(2, 1));
    REQUIRE(product(0, 1) == ExactCoefficient(3, 1));
    REQUIRE(aleph3::algebra::determinant(left) == ExactCoefficient(1, 1));
    const auto reduced = aleph3::algebra::row_reduce(left);
    REQUIRE(reduced == aleph3::algebra::identity_matrix<ExactCoefficient>(2));

    DenseMatrix<ExactCoefficient> large(
        1, 1, {{std::numeric_limits<std::int64_t>::max(), 1}});
    DenseMatrix<ExactCoefficient> two(1, 1, {{2, 1}});
    REQUIRE_THROWS_AS(aleph3::algebra::matrix_multiply(large, two), std::overflow_error);
}
