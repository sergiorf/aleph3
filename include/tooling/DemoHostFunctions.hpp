#pragma once

#include "sdk/Engine.hpp"
#include "sdk/Schema.hpp"

#include <span>
#include <string_view>

namespace aleph3::tooling {

struct DemoHostFunctionDoc {
    std::string_view name;
    std::string_view signature;
    std::string_view description;
    std::string_view cli_example;
    std::string_view repl_example;
};

void register_demo_host_functions(Engine& engine, Schema& schema);

[[nodiscard]] std::span<const std::string_view> demo_host_function_names() noexcept;
[[nodiscard]] std::span<const DemoHostFunctionDoc> demo_host_function_docs() noexcept;

}  // namespace aleph3::tooling
