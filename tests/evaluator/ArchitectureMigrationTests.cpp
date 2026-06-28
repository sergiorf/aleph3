#include "evaluator/BuiltInFunctions.hpp"
#include "evaluator/EvaluationContext.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "evaluator/Evaluator.hpp"
#include "kernel/Diagnostics.hpp"
#include "kernel/EvaluationContext.hpp"
#include "kernel/FunctionRegistry.hpp"
#include "kernel/Lowering.hpp"
#include "kernel/Rewrite.hpp"
#include "kernel/TrustedSubsetBridge.hpp"
#include "normalizer/Normalizer.hpp"
#include "packs/PackRegistry.hpp"
#include "parser/Parser.hpp"
#include "sdk/Policy.hpp"
#include "sdk/Types.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace aleph3;

TEST_CASE("EvaluationContext exposes symbol tables through compatibility aliases", "[architecture][symbols]") {
    EvaluationContext ctx;

    ctx.symbol_values.set("x", make_expr<Number>(2.0));
    REQUIRE(ctx.variables.contains("x"));
    REQUIRE(std::get<Number>(*ctx.variables.at("x")).value == 2.0);

    ctx.variables["y"] = make_expr<Number>(3.0);
    const ExprPtr* y = ctx.symbol_values.lookup("y");
    REQUIRE(y != nullptr);
    REQUIRE(std::get<Number>(**y).value == 3.0);
}

TEST_CASE("EvaluationContext copies preserve symbol and function state", "[architecture][symbols]") {
    EvaluationContext ctx;
    ctx.variables["offset"] = make_expr<Number>(5.0);
    evaluate(parse_expression("f[x_] := x + offset"), ctx);

    EvaluationContext copy = ctx;
    REQUIRE(copy.variables.contains("offset"));
    REQUIRE(copy.user_functions.contains("f"));

    auto result = evaluate(parse_expression("f[7]"), copy);
    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(std::get<Number>(*result).value == 12.0);
}

TEST_CASE("Built-in symbolic surface is registered through the pack registry", "[architecture][packs]") {
    register_built_in_functions();

    auto& registry = packs::PackRegistry::instance();
    REQUIRE(registry.has_function("StringJoin"));
    REQUIRE(registry.has_function("N"));
}

TEST_CASE("Kernel evaluation context can carry symbolic and SDK runtime state", "[architecture][kernel]") {
    Bindings bindings = {{"x", Value(2.0)}};
    Bindings constants = {{"pi", Value(3.14159)}};

    HostFunctionSpec clamp;
    clamp.name = "Clamp";
    clamp.arity = FunctionArity::exact(3);
    clamp.callback = [](std::span<const Value>) {
        EvaluationResult result;
        result.value = Value(0.0);
        return result;
    };

    std::unordered_map<std::string, HostFunctionSpec> host_functions;
    host_functions.emplace(clamp.name, clamp);

    Policy policy = Policy::default_policy();
    policy.budget().max_evaluation_steps = 77;

    kernel::EvaluationContext ctx(bindings, constants, host_functions, policy);
    ctx.symbol_values.set("offset", make_expr<Number>(5.0));
    ctx.symbol_metadata.set("Clamp", {
        "Clamp",
        {symbols::SymbolAttribute::numeric_function},
        "Clamp values into a numeric range.",
        symbols::DefinitionOrigin::builtin,
        "core"});
    ctx.definition_records.add("Clamp", {
        symbols::SymbolDefinitionKind::registered_handler,
        symbols::DefinitionOrigin::builtin,
        "core"});

    REQUIRE(ctx.variables.contains("offset"));
    REQUIRE(ctx.bindings().contains("x"));
    REQUIRE(ctx.constants().contains("pi"));
    REQUIRE(ctx.host_functions().contains("Clamp"));
    REQUIRE(ctx.policy().budget().max_evaluation_steps == 77);
    REQUIRE(ctx.symbol_metadata.contains("Clamp"));
    REQUIRE(ctx.definition_records.contains("Clamp"));
}

TEST_CASE("Kernel diagnostic taxonomy maps symbolic and runtime surfaces", "[architecture][kernel]") {
    REQUIRE(kernel::kernel_error_code_for(EvaluatorErrorKind::unsupported_construct) ==
            kernel::ErrorCode::unsupported_construct);
    REQUIRE(kernel::kernel_error_code_name(kernel::ErrorCode::unsupported_construct) ==
            "kernel.unsupported_construct");
    REQUIRE(kernel::sdk_runtime_projection_code(kernel::ErrorCode::type_mismatch) ==
            "runtime.type_mismatch");

    const auto runtime_error = kernel::make_runtime_error(
        kernel::ErrorCode::invalid_numeric_domain,
        "Sqrt is undefined for negative inputs.");
    REQUIRE(runtime_error.code == "runtime.invalid_numeric_domain");
}

TEST_CASE("Kernel function registry backs symbolic and host registration paths", "[architecture][kernel]") {
    auto& registry = kernel::FunctionRegistry::instance();
    REQUIRE(registry.has_function("StringJoin"));
    const auto* string_join = registry.find_symbolic_function_spec("StringJoin");
    REQUIRE(string_join != nullptr);
    REQUIRE(string_join->metadata.name == "StringJoin");
    REQUIRE(string_join->metadata.source == kernel::RegistrationSource::builtin);

    kernel::HostFunctionRegistry host_registry;
    HostFunctionSpec clamp;
    clamp.name = "Clamp";
    clamp.arity = FunctionArity::exact(3);
    clamp.callback = [](std::span<const Value>) {
        EvaluationResult result;
        result.value = Value(1.0);
        return result;
    };

    kernel::FunctionRegistry::register_host_function(host_registry, clamp);
    const auto* host_spec = kernel::FunctionRegistry::find_host_function(host_registry, "Clamp");
    REQUIRE(host_spec != nullptr);
    REQUIRE(host_spec->arity.allows(3));
}

TEST_CASE("Kernel function registry records minimal pack registration metadata", "[architecture][kernel][packs]") {
    kernel::FunctionRegistry registry;
    registry.register_pack_function(
        "core-algebra",
        "Expand",
        [](const FunctionCall& func, EvaluationContext&) -> ExprPtr {
            return make_expr<FunctionCall>(func.head, func.args);
        },
        "Expand products over sums.",
        true);

    const auto* spec = registry.find_symbolic_function_spec("Expand");
    REQUIRE(spec != nullptr);
    REQUIRE(spec->metadata.name == "Expand");
    REQUIRE(spec->metadata.owning_package == "core-algebra");
    REQUIRE(spec->metadata.documentation == "Expand products over sums.");
    REQUIRE(spec->metadata.source == kernel::RegistrationSource::pack);
    REQUIRE(spec->metadata.rewrite_safe);
}

TEST_CASE("Registered symbolic handlers win before user-defined functions", "[architecture][precedence]") {
    EvaluationContext ctx;
    evaluate(parse_expression("Length[x_] := 99"), ctx);

    auto result = evaluate(parse_expression("Length[{1, 2, 3}]"), ctx);
    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(std::get<Number>(*result).value == 3.0);
    REQUIRE(ctx.symbol_metadata.contains("Length"));
    REQUIRE(ctx.definition_records.contains("Length", symbols::SymbolDefinitionKind::registered_handler));
}

TEST_CASE("Builtin evaluator functions win before user-defined functions", "[architecture][precedence]") {
    EvaluationContext ctx;
    evaluate(parse_expression("Plus[x_, y_] := 99"), ctx);

    auto result = evaluate(parse_expression("Plus[1, 2]"), ctx);
    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(std::get<Number>(*result).value == 3.0);
    REQUIRE(ctx.symbol_metadata.contains("Plus"));
    REQUIRE(ctx.symbol_metadata.has_attribute("Plus", symbols::SymbolAttribute::listable));
    REQUIRE(ctx.symbol_metadata.has_attribute("Plus", symbols::SymbolAttribute::numeric_function));
    REQUIRE(ctx.definition_records.contains("Plus", symbols::SymbolDefinitionKind::builtin_function));
}

TEST_CASE("Unknown symbolic functions remain symbolic when no handler owns the name", "[architecture][precedence]") {
    EvaluationContext ctx;

    auto result = evaluate(parse_expression("f[x]"), ctx);
    REQUIRE(std::holds_alternative<FunctionCall>(*result));

    const auto& call = std::get<FunctionCall>(*result);
    REQUIRE(call.head == "f");
    REQUIRE(call.args.size() == 1);
}

TEST_CASE("Assignments populate kernel symbol metadata and definition records", "[architecture][symbols]") {
    EvaluationContext ctx;

    auto result = evaluate(parse_expression("answer = 42"), ctx);

    REQUIRE(std::holds_alternative<Symbol>(*result));
    REQUIRE(std::get<Symbol>(*result).name == "answer");
    REQUIRE(ctx.symbol_metadata.contains("answer"));
    REQUIRE(ctx.definition_records.contains("answer", symbols::SymbolDefinitionKind::own_value));
    REQUIRE(ctx.variables.contains("answer"));
    REQUIRE(std::holds_alternative<Number>(*ctx.variables.at("answer")));
}

TEST_CASE("User-defined functions populate kernel definition records", "[architecture][symbols]") {
    EvaluationContext ctx;

    auto result = evaluate(parse_expression("f[x_] := x + 1"), ctx);

    REQUIRE(std::holds_alternative<FunctionDefinition>(*result));
    REQUIRE(ctx.symbol_metadata.contains("f"));
    REQUIRE(ctx.definition_records.contains("f", symbols::SymbolDefinitionKind::user_function));
    REQUIRE(ctx.user_functions.contains("f"));
}

TEST_CASE("Host function definitions stay explicit and below builtin evaluator ownership", "[architecture][precedence]") {
    Bindings bindings;
    Bindings constants;
    kernel::HostFunctionRegistry host_functions;
    Policy policy = Policy::default_policy();
    EvaluationContext ctx(bindings, constants, host_functions, policy);

    HostFunctionSpec plus;
    plus.name = "Plus";
    plus.arity = FunctionArity::exact(2);
    plus.callback = [](std::span<const Value>) {
        EvaluationResult result;
        result.value = Value(99.0);
        return result;
    };
    kernel::FunctionRegistry::register_host_function(host_functions, plus);

    auto result = evaluate(parse_expression("Plus[1, 2]"), ctx);

    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(std::get<Number>(*result).value == 3.0);
    REQUIRE(ctx.definition_records.contains("Plus", symbols::SymbolDefinitionKind::builtin_function));
    REQUIRE(ctx.definition_records.contains("Plus", symbols::SymbolDefinitionKind::host_function));
}

TEST_CASE("Trusted-subset IR lowers into kernel Expr heads", "[architecture][lowering]") {
    using namespace aleph3::ir;

    const auto lowered = kernel::lower_trusted_ir_to_expr(make_node(
        {},
        BinaryOpNode{
            BinaryOperator::subtract,
            make_node({}, VariableNode{"x"}),
            make_node({}, NumberLiteralNode{2.0})}));

    REQUIRE(lowered.ok());
    REQUIRE(std::holds_alternative<FunctionCall>(*lowered.expr));

    const auto& plus = std::get<FunctionCall>(*lowered.expr);
    REQUIRE(plus.head == "Plus");
    REQUIRE(plus.args.size() == 2);
    REQUIRE(std::holds_alternative<Symbol>(*plus.args[0]));
    REQUIRE(std::get<Symbol>(*plus.args[0]).name == "x");

    REQUIRE(std::holds_alternative<FunctionCall>(*plus.args[1]));
    const auto& negated = std::get<FunctionCall>(*plus.args[1]);
    REQUIRE(negated.head == "Times");
    REQUIRE(negated.args.size() == 2);
    REQUIRE(std::holds_alternative<Number>(*negated.args[0]));
    REQUIRE(std::get<Number>(*negated.args[0]).value == -1.0);
    REQUIRE(std::holds_alternative<Number>(*negated.args[1]));
    REQUIRE(std::get<Number>(*negated.args[1]).value == 2.0);
}

TEST_CASE("Trusted-subset If nodes lower into kernel If calls", "[architecture][lowering]") {
    using namespace aleph3::ir;

    const auto lowered = kernel::lower_trusted_ir_to_expr(make_node(
        {},
        IfNode{
            make_node({}, BooleanLiteralNode{true}),
            make_node({}, NumberLiteralNode{1.0}),
            make_node({}, NumberLiteralNode{0.0})}));

    REQUIRE(lowered.ok());
    REQUIRE(std::holds_alternative<FunctionCall>(*lowered.expr));

    const auto& if_call = std::get<FunctionCall>(*lowered.expr);
    REQUIRE(if_call.head == "If");
    REQUIRE(if_call.args.size() == 3);
    REQUIRE(std::holds_alternative<Boolean>(*if_call.args[0]));
    REQUIRE(std::holds_alternative<Number>(*if_call.args[1]));
    REQUIRE(std::holds_alternative<Number>(*if_call.args[2]));
}

TEST_CASE("Trusted-subset bridge stages frontend and kernel forms together", "[architecture][lowering]") {
    using namespace aleph3::ir;

    const auto root = make_node(
        {},
        CallNode{"Clamp", {
            make_node({}, VariableNode{"x"}),
            make_node({}, NumberLiteralNode{0.0}),
            make_node({}, NumberLiteralNode{1.0})}});

    const auto staged = kernel::stage_trusted_subset_formula(root);

    REQUIRE(staged.ok());
    REQUIRE(std::holds_alternative<FunctionCall>(*staged.kernel_expr));

    const auto& call = std::get<FunctionCall>(*staged.kernel_expr);
    REQUIRE(call.head == "Clamp");
    REQUIRE(call.args.size() == 3);
}

TEST_CASE("Kernel rewrite can replace an exact structural match", "[architecture][rewrite]") {
    const auto expr = parse_expression("f[x]");
    const Rule rule{
        parse_expression("f[x]"),
        parse_expression("g[x]")
    };

    const auto result = kernel::rewrite_once(expr, rule);

    REQUIRE(result.changed);
    REQUIRE(result.rewrites_applied == 1);
    REQUIRE(to_string(result.expr) == "g[x]");
}

TEST_CASE("Kernel rewrite can traverse and apply repeated exact rewrites", "[architecture][rewrite]") {
    const auto expr = parse_expression("f[x] + f[x]");
    const Rule rule{
        parse_expression("f[x]"),
        parse_expression("g[x]")
    };

    const auto result = kernel::rewrite_repeated(expr, rule, 4);

    REQUIRE(result.changed);
    REQUIRE(result.rewrites_applied >= 2);
    REQUIRE(kernel::structurally_equal(result.expr, parse_expression("g[x] + g[x]")));
}

TEST_CASE("Kernel rewrite supports named pattern bindings", "[architecture][rewrite]") {
    const auto expr = parse_expression("f[x + 1]");
    const Rule rule{
        parse_expression("f[a_]"),
        parse_expression("g[a]")
    };

    const auto result = kernel::rewrite_once(expr, rule);

    REQUIRE(result.changed);
    REQUIRE(result.rewrites_applied == 1);
    REQUIRE(kernel::structurally_equal(result.expr, parse_expression("g[x + 1]")));
}

TEST_CASE("Kernel rewrite enforces repeated named pattern consistency", "[architecture][rewrite]") {
    const Rule rule{
        parse_expression("f[a_, a_]"),
        parse_expression("same[a]")
    };

    const auto matched = kernel::rewrite_once(parse_expression("f[x + 1, x + 1]"), rule);
    REQUIRE(matched.changed);
    REQUIRE(kernel::structurally_equal(matched.expr, parse_expression("same[x + 1]")));

    const auto mismatched = kernel::rewrite_once(parse_expression("f[x + 1, y + 1]"), rule);
    REQUIRE_FALSE(mismatched.changed);
    REQUIRE(kernel::structurally_equal(mismatched.expr, parse_expression("f[x + 1, y + 1]")));
}

TEST_CASE("Kernel rewrite can apply repeated named-pattern rewrites", "[architecture][rewrite]") {
    const auto expr = parse_expression("f[f[x]]");
    const Rule rule{
        parse_expression("f[a_]"),
        parse_expression("g[a]")
    };

    const auto result = kernel::rewrite_repeated(expr, rule, 4);

    REQUIRE(result.changed);
    REQUIRE(result.rewrites_applied >= 2);
    REQUIRE(kernel::structurally_equal(result.expr, parse_expression("g[g[x]]")));
}

TEST_CASE("Kernel rewrite supports normalized variadic Plus reduction", "[architecture][rewrite]") {
    kernel::EvaluationContext ctx;
    const auto normalized = normalize_expr(parse_expression("y + 0 + 2 + x + 3"));
    REQUIRE(std::holds_alternative<FunctionCall>(*normalized));

    const auto rewritten = kernel::rewrite_normalized_arithmetic_head(
        std::get<FunctionCall>(*normalized),
        ctx);

    REQUIRE(rewritten.has_value());
    REQUIRE(to_string(*rewritten) == "x + y + 5");
}

TEST_CASE("Kernel rewrite supports normalized exact variadic Times reduction", "[architecture][rewrite]") {
    kernel::EvaluationContext ctx;
    const auto normalized = normalize_expr(parse_expression("y * 1 * 2 * x * 1/3"));
    REQUIRE(std::holds_alternative<FunctionCall>(*normalized));

    const auto rewritten = kernel::rewrite_normalized_arithmetic_head(
        std::get<FunctionCall>(*normalized),
        ctx);

    REQUIRE(rewritten.has_value());
    REQUIRE(to_string(*rewritten) == "2/3 * x * y");
}

TEST_CASE("Kernel rewrite variadic arithmetic contract stays out of list-aware forms", "[architecture][rewrite]") {
    kernel::EvaluationContext ctx;
    const auto normalized = normalize_expr(parse_expression("0 * {x, y}"));
    REQUIRE(std::holds_alternative<FunctionCall>(*normalized));

    const auto rewritten = kernel::rewrite_normalized_arithmetic_head(
        std::get<FunctionCall>(*normalized),
        ctx);

    REQUIRE_FALSE(rewritten.has_value());
}

TEST_CASE("Kernel rewrite supports normalized symbolic coefficient collection", "[architecture][rewrite]") {
    kernel::EvaluationContext ctx;
    const auto normalized = normalize_expr(parse_expression("x + 2*x + 1/3*x + y"));
    REQUIRE(std::holds_alternative<FunctionCall>(*normalized));

    const auto rewritten = kernel::rewrite_normalized_symbolic_coefficient_head(
        std::get<FunctionCall>(*normalized),
        ctx);

    REQUIRE(rewritten.has_value());
    REQUIRE(to_string(*rewritten) == "y + 10/3 * x");
}

TEST_CASE("Kernel rewrite symbolic coefficient contract supports power bases", "[architecture][rewrite]") {
    kernel::EvaluationContext ctx;
    const auto normalized = normalize_expr(parse_expression("x^2 + 2*x^2"));
    REQUIRE(std::holds_alternative<FunctionCall>(*normalized));

    const auto rewritten = kernel::rewrite_normalized_symbolic_coefficient_head(
        std::get<FunctionCall>(*normalized),
        ctx);

    REQUIRE(rewritten.has_value());
    REQUIRE(to_string(*rewritten) == "3 * x^2");
}

TEST_CASE("Kernel rewrite symbolic coefficient contract stays out of multivariate products", "[architecture][rewrite]") {
    kernel::EvaluationContext ctx;
    const auto normalized = normalize_expr(parse_expression("x*y + 2*x*y"));
    REQUIRE(std::holds_alternative<FunctionCall>(*normalized));

    const auto rewritten = kernel::rewrite_normalized_symbolic_coefficient_head(
        std::get<FunctionCall>(*normalized),
        ctx);

    REQUIRE_FALSE(rewritten.has_value());
}

TEST_CASE("Kernel rewrite symbolic coefficient contract stays out of grouped symbolic bases", "[architecture][rewrite]") {
    kernel::EvaluationContext ctx;
    const auto normalized = normalize_expr(parse_expression("(x + y) + 2*(x + y)"));
    REQUIRE(std::holds_alternative<FunctionCall>(*normalized));

    const auto rewritten = kernel::rewrite_normalized_symbolic_coefficient_head(
        std::get<FunctionCall>(*normalized),
        ctx);

    REQUIRE_FALSE(rewritten.has_value());
}

TEST_CASE("Kernel rewrite symbolic coefficient contract stays out of call-shaped bases", "[architecture][rewrite]") {
    kernel::EvaluationContext ctx;
    const auto normalized = normalize_expr(parse_expression("f[x] + 2*f[x]"));
    REQUIRE(std::holds_alternative<FunctionCall>(*normalized));

    const auto rewritten = kernel::rewrite_normalized_symbolic_coefficient_head(
        std::get<FunctionCall>(*normalized),
        ctx);

    REQUIRE_FALSE(rewritten.has_value());
}

TEST_CASE("Kernel rewrite algebra-aware layer merges supported Times exponents", "[architecture][rewrite]") {
    kernel::EvaluationContext ctx;
    const auto normalized = normalize_expr(parse_expression("x * x^2 * y"));
    REQUIRE(std::holds_alternative<FunctionCall>(*normalized));

    const auto rewritten = kernel::rewrite_normalized_algebraic_head(
        std::get<FunctionCall>(*normalized),
        ctx);

    REQUIRE(rewritten.has_value());
    REQUIRE(to_string(*rewritten) == "x^3 * y");
}

TEST_CASE("Kernel rewrite algebra-aware layer collapses nested Power exponents", "[architecture][rewrite]") {
    kernel::EvaluationContext ctx;
    const auto normalized = normalize_expr(parse_expression("(x^2)^3"));
    REQUIRE(std::holds_alternative<FunctionCall>(*normalized));

    const auto rewritten = kernel::rewrite_normalized_algebraic_head(
        std::get<FunctionCall>(*normalized),
        ctx);

    REQUIRE(rewritten.has_value());
    REQUIRE(to_string(*rewritten) == "x^6");
}

TEST_CASE("Kernel rewrite owns fixed Power identities through a dedicated entrypoint", "[architecture][rewrite]") {
    kernel::EvaluationContext ctx;

    const auto zero_exponent = normalize_expr(parse_expression("x^0"));
    REQUIRE(std::holds_alternative<FunctionCall>(*zero_exponent));
    const auto zero_rewritten = kernel::rewrite_normalized_power_identity_head(
        std::get<FunctionCall>(*zero_exponent),
        ctx);
    REQUIRE(zero_rewritten.has_value());
    REQUIRE(to_string(*zero_rewritten) == "1");

    const auto unit_exponent = normalize_expr(parse_expression("x^1"));
    REQUIRE(std::holds_alternative<FunctionCall>(*unit_exponent));
    const auto unit_rewritten = kernel::rewrite_normalized_power_identity_head(
        std::get<FunctionCall>(*unit_exponent),
        ctx);
    REQUIRE(unit_rewritten.has_value());
    REQUIRE(to_string(*unit_rewritten) == "x");

    const auto unit_base = normalize_expr(parse_expression("1^z"));
    REQUIRE(std::holds_alternative<FunctionCall>(*unit_base));
    const auto unit_base_rewritten = kernel::rewrite_normalized_power_identity_head(
        std::get<FunctionCall>(*unit_base),
        ctx);
    REQUIRE(unit_base_rewritten.has_value());
    REQUIRE(to_string(*unit_base_rewritten) == "1");
}

TEST_CASE("Kernel Power identity entrypoint stays out of list-aware and non-identity forms", "[architecture][rewrite]") {
    kernel::EvaluationContext ctx;

    const auto list_power = normalize_expr(parse_expression("{x, y}^1"));
    REQUIRE(std::holds_alternative<FunctionCall>(*list_power));
    const auto list_rewritten = kernel::rewrite_normalized_power_identity_head(
        std::get<FunctionCall>(*list_power),
        ctx);
    REQUIRE_FALSE(list_rewritten.has_value());

    const auto domain_sensitive = normalize_expr(parse_expression("(-4)^0.5"));
    REQUIRE(std::holds_alternative<FunctionCall>(*domain_sensitive));
    const auto domain_rewritten = kernel::rewrite_normalized_power_identity_head(
        std::get<FunctionCall>(*domain_sensitive),
        ctx);
    REQUIRE_FALSE(domain_rewritten.has_value());
}

TEST_CASE("Kernel rewrite algebra-aware layer stays out of mixed symbolic bases", "[architecture][rewrite]") {
    kernel::EvaluationContext ctx;
    const auto normalized = normalize_expr(parse_expression("x * y * x*y"));
    REQUIRE(std::holds_alternative<FunctionCall>(*normalized));

    const auto rewritten = kernel::rewrite_normalized_algebraic_head(
        std::get<FunctionCall>(*normalized),
        ctx);

    REQUIRE_FALSE(rewritten.has_value());
}

TEST_CASE("Kernel rewrite algebra-aware layer stays out of symbolic nested Power exponents", "[architecture][rewrite]") {
    kernel::EvaluationContext ctx;
    const auto normalized = normalize_expr(parse_expression("(x^a)^b"));
    REQUIRE(std::holds_alternative<FunctionCall>(*normalized));

    const auto rewritten = kernel::rewrite_normalized_algebraic_head(
        std::get<FunctionCall>(*normalized),
        ctx);

    REQUIRE_FALSE(rewritten.has_value());
}

TEST_CASE("Kernel rewrite algebra-aware layer stays out of Divide forms", "[architecture][rewrite]") {
    kernel::EvaluationContext ctx;
    const auto normalized = normalize_expr(parse_expression("x / x"));
    REQUIRE(std::holds_alternative<FunctionCall>(*normalized));

    const auto rewritten = kernel::rewrite_normalized_algebraic_head(
        std::get<FunctionCall>(*normalized),
        ctx);

    REQUIRE_FALSE(rewritten.has_value());
}

TEST_CASE("Kernel rewrite algebra-aware layer stays out of power-domain-sensitive forms", "[architecture][rewrite]") {
    kernel::EvaluationContext ctx;
    const auto normalized = normalize_expr(parse_expression("(-4)^0.5"));
    REQUIRE(std::holds_alternative<FunctionCall>(*normalized));

    const auto rewritten = kernel::rewrite_normalized_algebraic_head(
        std::get<FunctionCall>(*normalized),
        ctx);

    REQUIRE_FALSE(rewritten.has_value());
}

TEST_CASE("Kernel rewrite entrypoints do not own special-function shortcuts", "[architecture][rewrite]") {
    kernel::EvaluationContext ctx;
    const auto normalized = normalize_expr(parse_expression("Gamma[x + 1]"));
    REQUIRE(std::holds_alternative<FunctionCall>(*normalized));

    const auto arithmetic = kernel::rewrite_normalized_arithmetic_head(
        std::get<FunctionCall>(*normalized),
        ctx);
    const auto coefficients = kernel::rewrite_normalized_symbolic_coefficient_head(
        std::get<FunctionCall>(*normalized),
        ctx);
    const auto algebraic = kernel::rewrite_normalized_algebraic_head(
        std::get<FunctionCall>(*normalized),
        ctx);

    REQUIRE_FALSE(arithmetic.has_value());
    REQUIRE_FALSE(coefficients.has_value());
    REQUIRE_FALSE(algebraic.has_value());
}

TEST_CASE("Kernel rewrite respects explicit rewrite-count bounds", "[architecture][rewrite]") {
    const auto expr = parse_expression("f[f[f[x]]]");
    const Rule rule{
        parse_expression("f[a_]"),
        parse_expression("g[a]")
    };

    const auto result = kernel::rewrite_repeated(expr, rule, 1);

    REQUIRE(result.changed);
    REQUIRE(result.rewrites_applied == 1);
    REQUIRE(kernel::structurally_equal(result.expr, parse_expression("g[f[f[x]]]")));
}

TEST_CASE("Kernel rewrite can consume runtime step budget through evaluation context", "[architecture][rewrite][budget]") {
    Policy policy = Policy::default_policy();
    policy.budget().max_evaluation_steps = 1;

    kernel::EvaluationContext ctx({}, {}, {}, policy);
    ctx.enable_runtime_strict_semantics(true);
    ctx.reset_runtime_step_counter();

    const auto expr = parse_expression("f[f[x]]");
    const Rule rule{
        parse_expression("f[a_]"),
        parse_expression("g[a]")
    };

    REQUIRE_THROWS_AS(kernel::rewrite_repeated(expr, rule, ctx, 4), kernel::RuntimeFailure);
}

TEST_CASE("Kernel rewrite does not normalize rewritten output implicitly", "[architecture][rewrite][normalize]") {
    const auto expr = parse_expression("f[y + x]");
    const Rule rule{
        parse_expression("f[a_]"),
        parse_expression("g[a]")
    };

    const auto rewritten = kernel::rewrite_once(expr, rule);

    REQUIRE(rewritten.changed);
    REQUIRE(to_string(rewritten.expr) == "g[y + x]");
    REQUIRE(to_string(normalize_expr(rewritten.expr)) == "g[x + y]");
}

TEST_CASE("Trusted-subset bridge evaluates lowered formulas with SDK bindings and constants", "[architecture][kernel]") {
    using namespace aleph3::ir;

    const auto root = make_node(
        {},
        IfNode{
            make_node({}, VariableNode{"enabled"}),
            make_node(
                {},
                BinaryOpNode{
                    BinaryOperator::add,
                    make_node({}, VariableNode{"x"}),
                    make_node({}, VariableNode{"offset"})}),
            make_node({}, NumberLiteralNode{0.0})});

    const auto staged = kernel::stage_trusted_subset_formula(root);
    REQUIRE(staged.ok());

    const Bindings bindings = {
        {"enabled", Value(true)},
        {"x", Value(2.0)}
    };
    const Bindings constants = {
        {"offset", Value(1.5)}
    };

    const auto result = kernel::evaluate_trusted_subset_formula(
        staged.kernel_expr,
        bindings,
        constants,
        {},
        Policy::default_policy());

    REQUIRE(result.ok());
    REQUIRE(result.value.has_value());
    REQUIRE(result.value->as_number() != nullptr);
    REQUIRE(*result.value->as_number() == 3.5);
}

TEST_CASE("Trusted-subset bridge projects strict runtime failures to SDK errors", "[architecture][kernel]") {
    using namespace aleph3::ir;

    const auto root = make_node(
        {},
        BinaryOpNode{
            BinaryOperator::divide,
            make_node({}, NumberLiteralNode{1.0}),
            make_node({}, VariableNode{"x"})});

    const auto staged = kernel::stage_trusted_subset_formula(root);
    REQUIRE(staged.ok());

    const auto result = kernel::evaluate_trusted_subset_formula(
        staged.kernel_expr,
        {{"x", Value(0.0)}},
        {},
        {},
        Policy::default_policy());

    REQUIRE_FALSE(result.ok());
    REQUIRE(result.error.has_value());
    REQUIRE(result.error->code == "runtime.division_by_zero");
}

TEST_CASE("Trusted-subset bridge executes registered host functions through the kernel registry", "[architecture][kernel]") {
    using namespace aleph3::ir;

    HostFunctionSpec scale_add;
    scale_add.name = "ScaleAdd";
    scale_add.arity = FunctionArity::exact(3);
    scale_add.parameters = {
        {"value", ValueType::number, true},
        {"scale", ValueType::number, true},
        {"offset", ValueType::number, true}
    };
    scale_add.return_type = ValueType::number;
    scale_add.callback = [](std::span<const Value> args) {
        EvaluationResult result;
        result.value = Value(*args[0].as_number() * *args[1].as_number() + *args[2].as_number());
        return result;
    };

    kernel::HostFunctionRegistry host_functions;
    kernel::FunctionRegistry::register_host_function(host_functions, scale_add);

    const auto root = make_node(
        {},
        CallNode{"ScaleAdd", {
            make_node({}, NumberLiteralNode{4.0}),
            make_node({}, NumberLiteralNode{1.5}),
            make_node({}, NumberLiteralNode{2.0})}});

    const auto staged = kernel::stage_trusted_subset_formula(root);
    REQUIRE(staged.ok());

    const auto result = kernel::evaluate_trusted_subset_formula(
        staged.kernel_expr,
        {},
        {},
        host_functions,
        Policy::default_policy());

    REQUIRE(result.ok());
    REQUIRE(result.value.has_value());
    REQUIRE(result.value->as_number() != nullptr);
    REQUIRE(*result.value->as_number() == 8.0);
}

TEST_CASE("Kernel evaluator can execute registered host functions through the shared context", "[architecture][kernel]") {
    EvaluationContext ctx;

    HostFunctionSpec scale_add;
    scale_add.name = "ScaleAdd";
    scale_add.arity = FunctionArity::exact(3);
    scale_add.parameters = {
        {"value", ValueType::number, true},
        {"scale", ValueType::number, true},
        {"offset", ValueType::number, true}
    };
    scale_add.return_type = ValueType::number;
    scale_add.callback = [](std::span<const Value> args) {
        EvaluationResult result;
        result.value = Value(*args[0].as_number() * *args[1].as_number() + *args[2].as_number());
        return result;
    };

    std::unordered_map<std::string, HostFunctionSpec> host_functions;
    kernel::FunctionRegistry::register_host_function(host_functions, scale_add);
    Policy policy = Policy::default_policy();

    ctx = kernel::EvaluationContext({}, {}, host_functions, policy);

    const auto result = evaluate(parse_expression("ScaleAdd[4, 1.5, 2]"), ctx);
    REQUIRE(std::holds_alternative<Number>(*result));
    REQUIRE(std::get<Number>(*result).value == 8.0);
}
