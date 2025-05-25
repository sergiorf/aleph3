#include "evaluator/BuiltInFunctions.hpp"

struct BuiltInFunctionsFixture {
    BuiltInFunctionsFixture() {
        aleph3::register_built_in_functions();
    }
};

// Register the fixture globally for all tests
static BuiltInFunctionsFixture global_builtins_fixture;