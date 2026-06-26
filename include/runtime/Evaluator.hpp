#pragma once

#include <cstddef>

#include "kernel/EvaluationContext.hpp"
#include "ir/Node.hpp"

namespace aleph3::runtime {

using EvaluationContext = aleph3::kernel::EvaluationContext;

class Evaluator {
public:
    explicit Evaluator(EvaluationContext context);

    [[nodiscard]] EvaluationResult evaluate(const ir::NodePtr& root) const;

private:
    EvaluationContext context_;
};

}  // namespace aleph3::runtime
