// evaluator/EvaluationContext.hpp
#pragma once

#include <unordered_map>
#include <string>

namespace mathix {

struct EvaluationContext {
    std::unordered_map<std::string, double> variables;
};

} // namespace mathix
