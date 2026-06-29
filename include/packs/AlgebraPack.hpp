/*
 * Algebra Pack Registration
 * -------------------------
 * Declares the first concrete pack-owned symbolic surface so callers can load
 * polynomial algebra handlers into a kernel function registry.
 */

#pragma once

#include "kernel/FunctionRegistry.hpp"

namespace aleph3::packs {

void register_algebra_pack(kernel::FunctionRegistry& registry);

}  // namespace aleph3::packs
