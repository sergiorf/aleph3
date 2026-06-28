/*
 * Built-in Registration Helpers
 * -----------------------------
 * Declares how the default Aleph3 symbolic surface is loaded into an explicit
 * kernel function registry for one engine, session, or test.
 */

#pragma once

#include "kernel/FunctionRegistry.hpp"

namespace aleph3 {

void register_built_in_functions(kernel::FunctionRegistry& registry);

}  // namespace aleph3
