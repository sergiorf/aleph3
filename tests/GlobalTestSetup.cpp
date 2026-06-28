#include "evaluator/BuiltInFunctions.hpp"

struct BuiltInFunctionsFixture {
    BuiltInFunctionsFixture() {
        (void)aleph3::kernel::default_function_registry();
    }
};

// Register the fixture globally for all tests
static BuiltInFunctionsFixture global_builtins_fixture;
