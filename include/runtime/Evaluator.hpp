#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

#include "ir/Node.hpp"
#include "sdk/Policy.hpp"
#include "sdk/Types.hpp"

namespace aleph3::runtime {

struct EvaluationContext {
    const Bindings& bindings;
    const std::unordered_map<std::string, HostFunctionSpec>& host_functions;
    const Policy& policy;
};

class Evaluator {
public:
    explicit Evaluator(EvaluationContext context);

    [[nodiscard]] EvaluationResult evaluate(const ir::NodePtr& root) const;

private:
    EvaluationContext context_;
};

}  // namespace aleph3::runtime
