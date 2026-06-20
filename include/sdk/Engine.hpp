#pragma once

#include <memory>
#include <string_view>

#include "sdk/Policy.hpp"
#include "sdk/Schema.hpp"
#include "sdk/Types.hpp"

namespace aleph3 {

class Engine {
public:
    explicit Engine(EngineOptions options = {});

    void register_function(HostFunctionSpec spec);

    [[nodiscard]] CompileResult compile(
        std::string_view source,
        const Schema& schema,
        const Policy& policy = Policy::default_policy()) const;

    [[nodiscard]] ValidationResult validate(
        std::string_view source,
        const Schema& schema,
        const Policy& policy = Policy::default_policy()) const;

    [[nodiscard]] EvaluationResult evaluate(
        const CompiledFormula& formula,
        const Bindings& bindings) const;

private:
    struct State;
    std::shared_ptr<State> state_;
};

}  // namespace aleph3
