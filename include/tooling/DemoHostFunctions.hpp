#pragma once

#include "sdk/Engine.hpp"
#include "sdk/Schema.hpp"

#include <span>
#include <string_view>

namespace aleph3::tooling {

void register_demo_host_functions(Engine& engine, Schema& schema);

[[nodiscard]] std::span<const std::string_view> demo_host_function_names() noexcept;

}  // namespace aleph3::tooling
