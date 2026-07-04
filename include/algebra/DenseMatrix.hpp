/*
 * Exact Dense Matrix
 * ------------------
 * Owns the algebra-layer dense matrix value type and storage-independent
 * algorithms used by the registered matrix surface.
 */
#pragma once

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace aleph3::algebra {

using MatrixWork = std::function<void()>;

template <typename Scalar>
class DenseMatrix {
public:
    DenseMatrix(std::size_t rows, std::size_t columns, std::vector<Scalar> values)
        : rows_(rows), columns_(columns), values_(std::move(values)) {
        if (rows_ == 0 || columns_ == 0 || columns_ > values_.max_size() / rows_ ||
            values_.size() != rows_ * columns_) {
            throw std::invalid_argument("DenseMatrix dimensions do not match its storage");
        }
    }

    [[nodiscard]] std::size_t rows() const noexcept { return rows_; }
    [[nodiscard]] std::size_t columns() const noexcept { return columns_; }
    [[nodiscard]] const std::vector<Scalar>& values() const noexcept { return values_; }

    [[nodiscard]] const Scalar& operator()(std::size_t row, std::size_t column) const {
        return values_.at(row * columns_ + column);
    }
    [[nodiscard]] Scalar& operator()(std::size_t row, std::size_t column) {
        return values_.at(row * columns_ + column);
    }

    bool operator==(const DenseMatrix&) const = default;

private:
    std::size_t rows_;
    std::size_t columns_;
    std::vector<Scalar> values_;
};

inline void matrix_work(const MatrixWork& work) {
    if (work) work();
}

template <typename Scalar>
DenseMatrix<Scalar> matrix_add(
    const DenseMatrix<Scalar>& left,
    const DenseMatrix<Scalar>& right,
    const MatrixWork& work = {}) {
    if (left.rows() != right.rows() || left.columns() != right.columns()) {
        throw std::domain_error("Matrix addition requires equal dimensions");
    }
    std::vector<Scalar> result;
    result.reserve(left.values().size());
    for (std::size_t i = 0; i < left.values().size(); ++i) {
        matrix_work(work);
        result.push_back(left.values()[i] + right.values()[i]);
    }
    return DenseMatrix<Scalar>(left.rows(), left.columns(), std::move(result));
}

template <typename Scalar>
DenseMatrix<Scalar> matrix_multiply(
    const DenseMatrix<Scalar>& left,
    const DenseMatrix<Scalar>& right,
    const MatrixWork& work = {}) {
    if (left.columns() != right.rows()) {
        throw std::domain_error("Matrix multiplication requires compatible inner dimensions");
    }
    std::vector<Scalar> result(left.rows() * right.columns(), Scalar::zero());
    for (std::size_t row = 0; row < left.rows(); ++row) {
        for (std::size_t column = 0; column < right.columns(); ++column) {
            for (std::size_t inner = 0; inner < left.columns(); ++inner) {
                matrix_work(work);
                result[row * right.columns() + column] =
                    result[row * right.columns() + column] + left(row, inner) * right(inner, column);
            }
        }
    }
    return DenseMatrix<Scalar>(left.rows(), right.columns(), std::move(result));
}

template <typename Scalar>
DenseMatrix<Scalar> identity_matrix(std::size_t size) {
    if (size == 0) throw std::invalid_argument("Identity matrix size must be positive");
    std::vector<Scalar> result(size * size, Scalar::zero());
    for (std::size_t i = 0; i < size; ++i) result[i * size + i] = Scalar::one();
    return DenseMatrix<Scalar>(size, size, std::move(result));
}

template <typename Scalar>
DenseMatrix<Scalar> matrix_transpose(const DenseMatrix<Scalar>& matrix) {
    std::vector<Scalar> result(matrix.values().size(), Scalar::zero());
    for (std::size_t row = 0; row < matrix.rows(); ++row) {
        for (std::size_t column = 0; column < matrix.columns(); ++column) {
            result[column * matrix.rows() + row] = matrix(row, column);
        }
    }
    return DenseMatrix<Scalar>(matrix.columns(), matrix.rows(), std::move(result));
}

template <typename Scalar>
DenseMatrix<Scalar> row_reduce(DenseMatrix<Scalar> matrix, const MatrixWork& work = {}) {
    std::size_t pivot_row = 0;
    for (std::size_t column = 0; column < matrix.columns() && pivot_row < matrix.rows(); ++column) {
        std::size_t pivot = pivot_row;
        while (pivot < matrix.rows() && matrix(pivot, column).is_zero()) ++pivot;
        if (pivot == matrix.rows()) continue;
        if (pivot != pivot_row) {
            for (std::size_t c = 0; c < matrix.columns(); ++c) std::swap(matrix(pivot, c), matrix(pivot_row, c));
        }
        const Scalar divisor = matrix(pivot_row, column);
        for (std::size_t c = 0; c < matrix.columns(); ++c) {
            matrix_work(work);
            matrix(pivot_row, c) = matrix(pivot_row, c) / divisor;
        }
        for (std::size_t row = 0; row < matrix.rows(); ++row) {
            if (row == pivot_row || matrix(row, column).is_zero()) continue;
            const Scalar factor = matrix(row, column);
            for (std::size_t c = 0; c < matrix.columns(); ++c) {
                matrix_work(work);
                matrix(row, c) = matrix(row, c) - factor * matrix(pivot_row, c);
            }
        }
        ++pivot_row;
    }
    return matrix;
}

template <typename Scalar>
Scalar determinant(DenseMatrix<Scalar> matrix, const MatrixWork& work = {}) {
    if (matrix.rows() != matrix.columns()) throw std::domain_error("Determinant requires a square matrix");
    Scalar result = Scalar::one();
    bool negate = false;
    for (std::size_t column = 0; column < matrix.columns(); ++column) {
        std::size_t pivot = column;
        while (pivot < matrix.rows() && matrix(pivot, column).is_zero()) ++pivot;
        if (pivot == matrix.rows()) return Scalar::zero();
        if (pivot != column) {
            for (std::size_t c = 0; c < matrix.columns(); ++c) std::swap(matrix(pivot, c), matrix(column, c));
            negate = !negate;
        }
        const Scalar pivot_value = matrix(column, column);
        matrix_work(work);
        result = result * pivot_value;
        for (std::size_t row = column + 1; row < matrix.rows(); ++row) {
            if (matrix(row, column).is_zero()) continue;
            matrix_work(work);
            const Scalar factor = matrix(row, column) / pivot_value;
            for (std::size_t c = column; c < matrix.columns(); ++c) {
                matrix_work(work);
                matrix(row, c) = matrix(row, c) - factor * matrix(column, c);
            }
        }
    }
    return negate ? Scalar::zero() - result : result;
}

}  // namespace aleph3::algebra
