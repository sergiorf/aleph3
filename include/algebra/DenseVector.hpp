/*
 * Exact Dense Vector
 * ------------------
 * Owns the algebra-layer dense vector value type and storage-independent
 * algorithms used by the registered exact vector surface.
 */
#pragma once

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace aleph3::algebra {

using AlgebraWork = std::function<void()>;

template <typename Scalar>
class DenseVector {
public:
    explicit DenseVector(std::vector<Scalar> values)
        : values_(std::move(values)) {
        if (values_.empty()) {
            throw std::invalid_argument("DenseVector storage must be non-empty");
        }
    }

    [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }
    [[nodiscard]] const Scalar& operator[](std::size_t index) const { return values_.at(index); }
    [[nodiscard]] Scalar& operator[](std::size_t index) { return values_.at(index); }
    [[nodiscard]] const std::vector<Scalar>& values() const noexcept { return values_; }

    bool operator==(const DenseVector&) const = default;

private:
    std::vector<Scalar> values_;
};

inline void algebra_work(const AlgebraWork& work) {
    if (work) work();
}

template <typename Scalar>
Scalar dot_product(
    const DenseVector<Scalar>& left,
    const DenseVector<Scalar>& right,
    const AlgebraWork& work = {}) {
    if (left.size() != right.size()) {
        throw std::domain_error("Dot requires vectors of equal length");
    }
    Scalar result = Scalar::zero();
    for (std::size_t i = 0; i < left.size(); ++i) {
        algebra_work(work);
        result = result + left[i] * right[i];
    }
    return result;
}

template <typename Scalar>
DenseVector<Scalar> cross_product(
    const DenseVector<Scalar>& left,
    const DenseVector<Scalar>& right,
    const AlgebraWork& work = {}) {
    if (left.size() != 3 || right.size() != 3) {
        throw std::domain_error("Cross requires two three-dimensional vectors");
    }
    std::vector<Scalar> result;
    result.reserve(3);

    algebra_work(work);
    result.push_back(left[1] * right[2] - left[2] * right[1]);
    algebra_work(work);
    result.push_back(left[2] * right[0] - left[0] * right[2]);
    algebra_work(work);
    result.push_back(left[0] * right[1] - left[1] * right[0]);
    return DenseVector<Scalar>(std::move(result));
}

template <typename Scalar>
Scalar squared_norm(
    const DenseVector<Scalar>& vector,
    const AlgebraWork& work = {}) {
    return dot_product(vector, vector, work);
}

}  // namespace aleph3::algebra
