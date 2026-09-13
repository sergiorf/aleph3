/* Algebra-layer value and algorithm tests for exact dense matrices. */
#include "algebra/DenseMatrix.hpp"
#include "algebra/DenseVector.hpp"
#include "algebra/ExactPolynomial.hpp"

#include <catch2/catch_test_macros.hpp>

using aleph3::ExactCoefficient;
using aleph3::algebra::DenseMatrix;
using aleph3::algebra::DenseVector;

TEST_CASE("DenseMatrix owns exact row-major value storage", "[algebra][matrix]") {
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

    DenseMatrix<ExactCoefficient> rational(2, 2, {{1, 2}, {1, 1}, {1, 1}, {3, 1}});
    REQUIRE(aleph3::algebra::determinant(rational) == ExactCoefficient(1, 2));
    const auto rational_reduced = aleph3::algebra::row_reduce(rational);
    REQUIRE(rational_reduced == aleph3::algebra::identity_matrix<ExactCoefficient>(2));

    DenseMatrix<ExactCoefficient> large(
        1,
        1,
        {{aleph3::kernel::ExactInteger::from_decimal_string("9223372036854775808"),
          aleph3::kernel::ExactInteger(1)}});
    DenseMatrix<ExactCoefficient> two(1, 1, {{2, 1}});
    const auto large_product = aleph3::algebra::matrix_multiply(large, two);
    REQUIRE(large_product(0, 0) == ExactCoefficient(
        aleph3::kernel::ExactInteger::from_decimal_string("18446744073709551616"),
        aleph3::kernel::ExactInteger(1)));
}

TEST_CASE("DenseVector owns exact flat value storage", "[algebra][vector]") {
    DenseVector<ExactCoefficient> vector({{1, 1}, {2, 1}, {3, 1}});
    REQUIRE(vector.size() == 3);
    REQUIRE(vector[1] == ExactCoefficient(2, 1));
    REQUIRE(vector.values()[2] == ExactCoefficient(3, 1));
    REQUIRE_THROWS_AS(DenseVector<ExactCoefficient>(std::vector<ExactCoefficient>{}), std::invalid_argument);
}

TEST_CASE("Dense vector algorithms preserve exact arithmetic", "[algebra][vector][exact]") {
    DenseVector<ExactCoefficient> left({{1, 1}, {2, 1}, {3, 1}});
    DenseVector<ExactCoefficient> right({{4, 1}, {5, 1}, {6, 1}});
    REQUIRE(aleph3::algebra::dot_product(left, right) == ExactCoefficient(32, 1));
    REQUIRE(aleph3::algebra::dot_product(right, left) == ExactCoefficient(32, 1));

    const auto cross = aleph3::algebra::cross_product(left, right);
    REQUIRE(cross == DenseVector<ExactCoefficient>({{-3, 1}, {6, 1}, {-3, 1}}));
    REQUIRE(aleph3::algebra::dot_product(cross, left) == ExactCoefficient(0, 1));
    REQUIRE(aleph3::algebra::dot_product(cross, right) == ExactCoefficient(0, 1));

    DenseVector<ExactCoefficient> rational({{1, 2}, {2, 3}});
    REQUIRE(aleph3::algebra::squared_norm(rational) == ExactCoefficient(25, 36));

    DenseVector<ExactCoefficient> large(
        {{aleph3::kernel::ExactInteger::from_decimal_string("9223372036854775808"),
          aleph3::kernel::ExactInteger(1)}});
    DenseVector<ExactCoefficient> two({{2, 1}});
    REQUIRE(aleph3::algebra::dot_product(large, two) == ExactCoefficient(
        aleph3::kernel::ExactInteger::from_decimal_string("18446744073709551616"),
        aleph3::kernel::ExactInteger(1)));
}
