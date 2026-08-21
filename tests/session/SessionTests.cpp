#include "session/Session.hpp"

#include <catch2/catch_test_macros.hpp>

#include "help/HelpTexts.hpp"
#include "kernel/FunctionRegistry.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>

using aleph3::session::Session;
using aleph3::session::SessionOperation;

TEST_CASE("Session preserves definitions across evaluations", "[session]") {
    Session session;
    REQUIRE(session.execute({"a = 2"}).ok);
    const auto value = session.execute({"a + 3"});
    REQUIRE(value.ok);
    REQUIRE(value.output == "5");

    REQUIRE(session.execute({"f[x_] := x + 1"}).ok);
    REQUIRE(session.execute({"f[4]"}).output == "5");
}

TEST_CASE("Session instances isolate state", "[session]") {
    Session left;
    Session right;
    REQUIRE(left.execute({"a = 2"}).ok);
    REQUIRE(left.execute({"a"}).output == "2");
    REQUIRE(right.execute({"a"}).output == "a");
}

TEST_CASE("Session reset discards local definitions and preserves providers", "[session][reset]") {
    Session session;
    REQUIRE(session.execute({"a = 2"}).ok);
    REQUIRE(session.execute({"f[x_] := x + 1"}).ok);
    REQUIRE(session.execute({"Plus[x_, y_] := 99"}).ok);
    REQUIRE(session.execute({"a"}).output == "2");
    REQUIRE(session.execute({"f[4]"}).output == "5");
    REQUIRE(session.execute({"1 + 2"}).output == "3");

    REQUIRE(session.execute({"a", SessionOperation::complete}).completions.size() == 1);
    REQUIRE(session.execute({"f", SessionOperation::complete}).completions.size() == 1);

    session.reset();

    REQUIRE(session.execute({"a"}).output == "a");
    REQUIRE(session.execute({"f[4]"}).output == "f[4]");
    REQUIRE(session.execute({"a", SessionOperation::complete}).completions.empty());
    REQUIRE(session.execute({"f", SessionOperation::complete}).completions.empty());

    REQUIRE(session.execute({"1 + 2"}).output == "3");
    REQUIRE(session.execute({"Length[{1, 2}]"}).output == "2");
    REQUIRE(session.execute({"Factor[x^2 - 1]"}).output == "(x - 1) * (x + 1)");
    REQUIRE(session.execute({"D[x^2, x]"}).output == "2 * x");

    const auto pack = session.execute({"Fac", SessionOperation::complete});
    REQUIRE(pack.completions.size() == 1);
    REQUIRE(pack.completions.front().name == "Factor");
    REQUIRE(pack.completions.front().category == "pack");
}

TEST_CASE("Session cleanup updates evaluation, completion, and help consistently", "[session][cleanup][completion][help]") {
    Session session;

    REQUIRE(session.execute({"a = 2"}).ok);
    REQUIRE(session.execute({"f[x_] := x + a"}).ok);
    REQUIRE(session.execute({"Plus[x_, y_] := 99"}).ok);

    REQUIRE(session.execute({"f[3]"}).output == "5");
    REQUIRE(session.execute({"1 + 2"}).output == "3");
    REQUIRE(session.execute({"a", SessionOperation::complete}).completions.front().category == "symbol");
    REQUIRE(session.execute({"f", SessionOperation::help}).help_entries.front().category == "function");
    REQUIRE(session.execute({"Plus", SessionOperation::complete}).completions.front().category == "builtin");

    REQUIRE(session.execute({"Clear[a]"}).ok);
    REQUIRE(session.execute({"f[3]"}).output == "a + 3");
    REQUIRE(session.execute({"a", SessionOperation::complete}).completions.empty());
    REQUIRE(session.execute({"f", SessionOperation::complete}).completions.front().category == "function");

    REQUIRE(session.execute({"Unset[f]"}).ok);
    REQUIRE(session.execute({"f[3]"}).output == "a + 3");
    REQUIRE(session.execute({"f", SessionOperation::complete}).completions.front().category == "function");

    REQUIRE(session.execute({"Clear[f]"}).ok);
    REQUIRE(session.execute({"f[3]"}).output == "f[3]");
    REQUIRE(session.execute({"f", SessionOperation::help}).help_entries.empty());
}

TEST_CASE("Session reset clears assumptions and session-local provider shadows", "[session][reset][assumptions]") {
    Session session;

    REQUIRE(session.execute({"Assuming[x > 0, Positive[x]]"}).output == "True");
    REQUIRE(session.execute({"Plus = 7"}).ok);
    REQUIRE(session.execute({"Plus[x_, y_] := 99"}).ok);
    REQUIRE(session.execute({"Plus"}).output == "7");
    REQUIRE(session.execute({"1 + 2"}).output == "3");
    REQUIRE(session.execute({"Plus", SessionOperation::complete}).completions.front().category == "builtin");

    session.reset();

    REQUIRE(session.execute({"Plus"}).output == "Plus");
    REQUIRE(session.execute({"1 + 2"}).output == "3");
    REQUIRE(session.execute({"D[x^2, x]"}).output == "2 * x");
    REQUIRE(session.execute({"Plus", SessionOperation::complete}).completions.front().category == "builtin");
}

TEST_CASE("Session returns structured failures and supports all operations", "[session]") {
    Session session;
    const auto empty = session.execute({""});
    REQUIRE_FALSE(empty.ok);
    REQUIRE(empty.diagnostics.front().code == "session.empty_source");

    const auto failure = session.execute({"("});
    REQUIRE_FALSE(failure.ok);
    REQUIRE(failure.diagnostics.front().code == "session.parse_error");

    REQUIRE(session.execute({"a = 4"}).ok);
    const auto unsupported = session.execute({"MatchQ[2, _Matrix]"});
    REQUIRE_FALSE(unsupported.ok);
    REQUIRE_FALSE(unsupported.diagnostics.front().code.empty());
    REQUIRE(session.execute({"a + 1"}).output == "5");

    REQUIRE(session.execute({"0 + x", SessionOperation::simplify}).output == "x");
    REQUIRE(session.execute({"f[x]", SessionOperation::full_form}).output == "f[x]");
}

TEST_CASE("Session inspects expressions without mutating state", "[session][inspection]") {
    Session session;
    REQUIRE(session.execute({"a = 2"}).ok);

    const auto result = session.execute({"f[a, x + 1]", SessionOperation::inspect});
    REQUIRE(result.ok);
    REQUIRE(result.inspections.size() == 1);
    REQUIRE(result.inspections.front().head == "f");
    REQUIRE(result.inspections.front().node_count == 5);
    REQUIRE(result.inspections.front().depth == 3);
    REQUIRE(result.inspections.front().symbols == std::vector<std::string>{"a", "x"});
    REQUIRE(session.execute({"a"}).output == "2");
}

TEST_CASE("Session discovers registered packs deterministically", "[session][packs]") {
    Session session;
    const auto result = session.execute({"", SessionOperation::discover_packs});
    REQUIRE(result.ok);
    REQUIRE(result.packs.size() == 2);
    REQUIRE(result.packs[0].name == "core-algebra");
    REQUIRE(result.packs[0].symbols ==
        std::vector<std::string>{"Cancel", "Coefficient", "CoefficientList", "Collect", "Denominator", "Det", "Expand", "Factor", "GCD",
            "IdentityMatrix", "LinearSolve", "MatrixAdd", "MatrixMultiply", "Numerator", "PolynomialQuotient", "RowReduce",
            "Together", "Transpose"});
    REQUIRE(result.packs[1].name == "core-calculus");
    REQUIRE(result.packs[1].symbols == std::vector<std::string>{"D", "Differentiate"});
}

TEST_CASE("Session exposes focused differentiation values and diagnostics", "[session][calculus]") {
    Session session;
    REQUIRE(session.execute({"D[x^2 + 3*x, x]"}).output == "2 * x + 3");
    REQUIRE(session.execute({"D[f[x], x]"}).output == "D[f[x], x]");

    const auto failure = session.execute({"D[x, x + 1]"});
    REQUIRE_FALSE(failure.ok);
    REQUIRE(failure.diagnostics.size() == 1);
    REQUIRE(failure.diagnostics.front().code == "kernel.invalid_form");
}

TEST_CASE("Session exposes variable dependency inspection values and diagnostics", "[session][variables]") {
    Session session;
    REQUIRE(session.execute({"FreeVariables[f[a_] -> g[a, y]]"}).output == "{y}");
    REQUIRE(session.execute({"BoundVariables[f[a_, b_Integer] -> g[a, b, y]]"}).output == "{a, b}");
    REQUIRE(session.execute({"DependsOn[f[a_] -> g[a, y], a]"}).output == "False");
    REQUIRE(session.execute({"DependsOn[f[a_] -> g[a, y], y]"}).output == "True");

    const auto failure = session.execute({"DependsOn[x + y, x + 1]"});
    REQUIRE_FALSE(failure.ok);
    REQUIRE(failure.diagnostics.size() == 1);
    REQUIRE(failure.diagnostics.front().code == "kernel.invalid_form");
}

TEST_CASE("Session exposes exact matrix values and diagnostics", "[session][algebra][matrix]") {
    Session session;
    REQUIRE(session.execute({"Det[{{1, 2}, {3, 4}}]"}).output == "-2");
    REQUIRE(session.execute({"LinearSolve[{{2, 1}, {1, -1}}, {5, 1}]"}).output == "{2, 1}");
    const auto failure = session.execute({"MatrixMultiply[{{1, 2}}, {{1, 2}}]"});
    REQUIRE_FALSE(failure.ok);
    REQUIRE(failure.diagnostics.front().code == "runtime.domain_violation");
}

TEST_CASE("Session completes registry and session symbols deterministically", "[session][completion]") {
    Session session;
    REQUIRE(session.execute({"alpha = 2"}).ok);
    REQUIRE(session.execute({"Factor[x_] := x"}).ok);

    const auto factor = session.execute({"Fa", SessionOperation::complete});
    REQUIRE(factor.ok);
    REQUIRE(factor.completions.size() == 1);
    REQUIRE(factor.completions.front().name == "Factor");
    REQUIRE(factor.completions.front().category == "pack");
    REQUIRE(factor.completions.front().owning_package == "core-algebra");
    REQUIRE(factor.completions.front().documentation == "Factor a supported exact polynomial expression.");

    const auto packs = session.execute({"Pol", SessionOperation::complete});
    REQUIRE(packs.completions.size() == 1);
    REQUIRE(packs.completions.front().name == "PolynomialQuotient");
    REQUIRE(packs.completions.front().category == "pack");
    REQUIRE(packs.completions.front().owning_package == "core-algebra");

    const auto derivative = session.execute({"Dif", SessionOperation::complete});
    REQUIRE(derivative.completions.size() == 1);
    REQUIRE(derivative.completions.front().name == "Differentiate");
    REQUIRE(derivative.completions.front().category == "pack");
    REQUIRE(derivative.completions.front().owning_package == "core-calculus");

    const auto builtin = session.execute({"Abs", SessionOperation::complete});
    REQUIRE(builtin.completions.size() == 1);
    REQUIRE(builtin.completions.front().category == "builtin");

    const auto replace_all = session.execute({"ReplaceA", SessionOperation::complete});
    REQUIRE(replace_all.completions.size() == 1);
    REQUIRE(replace_all.completions.front().name == "ReplaceAll");
    REQUIRE(replace_all.completions.front().category == "builtin");

    const auto special_form = session.execute({"And", SessionOperation::complete});
    REQUIRE(special_form.completions.size() == 1);
    REQUIRE(special_form.completions.front().category == "special-form");

    const auto symbol = session.execute({"al", SessionOperation::complete});
    REQUIRE(symbol.completions.size() == 1);
    REQUIRE(symbol.completions.front().category == "symbol");
    REQUIRE(session.execute({"alpha"}).output == "2");

    const auto all = session.execute({"", SessionOperation::complete});
    REQUIRE(std::is_sorted(
        all.completions.begin(),
        all.completions.end(),
        [](const auto& left, const auto& right) { return left.name < right.name; }));
}

TEST_CASE("Session completion is isolated and permits empty results", "[session][completion]") {
    Session left;
    Session right;
    REQUIRE(left.execute({"localName = 1"}).ok);
    REQUIRE(left.execute({"local", SessionOperation::complete}).completions.size() == 1);
    REQUIRE(right.execute({"local", SessionOperation::complete}).completions.empty());
    REQUIRE(right.execute({"NoSuchPrefix", SessionOperation::complete}).ok);
}

TEST_CASE("Session help exposes provider and session-local discovery", "[session][help][completion]") {
    Session session;

    const auto factor = session.execute({"Factor", SessionOperation::help});
    REQUIRE(factor.ok);
    REQUIRE(factor.help_entries.size() == 1);
    REQUIRE(factor.help_entries.front().name == "Factor");
    REQUIRE(factor.help_entries.front().category == "pack");
    REQUIRE(factor.help_entries.front().owning_package == "core-algebra");
    REQUIRE(factor.help_entries.front().examples.size() == 1);

    const auto derivative = session.execute({"D", SessionOperation::help});
    REQUIRE(derivative.ok);
    REQUIRE(derivative.help_entries.size() == 1);
    REQUIRE(derivative.help_entries.front().owning_package == "core-calculus");

    const auto clear = session.execute({"Clear", SessionOperation::help});
    REQUIRE(clear.ok);
    REQUIRE(clear.help_entries.size() == 1);
    REQUIRE(clear.help_entries.front().unsupported.find("cannot remove") != std::string::npos);

    const auto unknown = session.execute({"NoSuchHelpPrefix", SessionOperation::help});
    REQUIRE(unknown.ok);
    REQUIRE(unknown.help_entries.empty());

    REQUIRE(session.execute({"alpha = 2"}).ok);
    REQUIRE(session.execute({"f[x_] := x + 1"}).ok);
    const auto alpha = session.execute({"alpha", SessionOperation::complete});
    REQUIRE(alpha.completions.size() == 1);
    REQUIRE(alpha.completions.front().documentation == "session-local own value");

    const auto function = session.execute({"f", SessionOperation::help});
    REQUIRE(function.help_entries.size() == 1);
    REQUIRE(function.help_entries.front().category == "function");
    REQUIRE(function.help_entries.front().description == "session-local user function");

    REQUIRE(session.execute({"Plus[x_, y_] := 99"}).ok);
    const auto plus = session.execute({"Plus", SessionOperation::complete});
    REQUIRE(plus.completions.size() == 1);
    REQUIRE(plus.completions.front().category == "builtin");

    REQUIRE(session.execute({"Clear[f]"}).ok);
    REQUIRE(session.execute({"f", SessionOperation::complete}).completions.empty());

    session.reset();
    REQUIRE(session.execute({"alpha", SessionOperation::help}).help_entries.empty());
    REQUIRE(session.execute({"Factor", SessionOperation::help}).help_entries.size() == 1);
}

TEST_CASE("Help catalog has rich metadata for registered provider entries", "[session][help][completion]") {
    std::map<std::string, std::size_t> help_name_counts;
    for (const auto& entry : aleph3::get_help_entries()) {
        ++help_name_counts[entry.name];
    }
    for (const auto& [name, count] : help_name_counts) {
        INFO("help entry: " << name);
        REQUIRE(count == 1);
    }

    const std::set<std::string> no_example_required = {"RuleDelayed"};
    for (const auto& callable : aleph3::kernel::default_function_registry().callable_metadata()) {
        const auto* help = aleph3::find_help_entry(callable.metadata.name);
        INFO("registered symbol: " << callable.metadata.name);
        REQUIRE(help != nullptr);
        REQUIRE_FALSE(help->description.empty());
        REQUIRE_FALSE(help->forms.empty());
        if (!no_example_required.contains(help->name)) {
            REQUIRE_FALSE(help->examples.empty());
        }
        REQUIRE_FALSE(help->exactness.empty());
        REQUIRE_FALSE(help->unsupported.empty());
        REQUIRE_FALSE(help->owning_component.empty());
        REQUIRE_FALSE(help->manual_anchor.empty());
    }
}

TEST_CASE("Session help exposes rich entries across supported discovery groups", "[session][help]") {
    Session session;
    const std::vector<std::string> names = {
        "Log", "ArcTan", "Length", "Assuming", "MatrixAdd", "D", "If", "Rule"
    };

    for (const auto& name : names) {
        const auto result = session.execute({name, SessionOperation::help});
        INFO("help lookup: " << name);
        REQUIRE(result.ok);
        REQUIRE(result.help_entries.size() == 1);
        const auto& entry = result.help_entries.front();
        REQUIRE(entry.name == name);
        REQUIRE_FALSE(entry.forms.empty());
        if (name != "RuleDelayed") {
            REQUIRE_FALSE(entry.examples.empty());
        }
        REQUIRE_FALSE(entry.exactness.empty());
        REQUIRE_FALSE(entry.unsupported.empty());
        REQUIRE_FALSE(entry.manual_anchor.empty());
    }

    const auto log = session.execute({"Log", SessionOperation::help});
    REQUIRE(log.help_entries.front().forms == std::vector<std::string>{"Log[x]", "Log[base, x]"});

    const auto arctan = session.execute({"ArcTan", SessionOperation::help});
    REQUIRE(arctan.help_entries.front().forms == std::vector<std::string>{"ArcTan[x]", "ArcTan[x, y]"});
}

TEST_CASE("Session reports polynomial division by zero with a stable diagnostic", "[session][algebra][diagnostics]") {
    Session session;
    const auto result = session.execute({"PolynomialQuotient[x, 0, x]"});
    REQUIRE_FALSE(result.ok);
    REQUIRE(result.diagnostics.size() == 1);
    REQUIRE(result.diagnostics.front().code == "runtime.division_by_zero");
}

TEST_CASE("Session exposes bounded multivariate GCD values and diagnostics", "[session][algebra][gcd]") {
    Session session;
    REQUIRE(session.execute({"GCD[x*y + x, x, {x, y}]"}).output == "x");

    const auto unsupported = session.execute({"GCD[x + y, x - y, {x, y}]"});
    REQUIRE_FALSE(unsupported.ok);
    REQUIRE(unsupported.diagnostics.size() == 1);
    REQUIRE(unsupported.diagnostics.front().code == "kernel.unsupported_construct");

    const auto domain = session.execute({"GCD[0, 0, {x, y}]"});
    REQUIRE_FALSE(domain.ok);
    REQUIRE(domain.diagnostics.size() == 1);
    REQUIRE(domain.diagnostics.front().code == "kernel.domain_violation");
}

TEST_CASE("Session exposes exact coefficient extraction values and diagnostics", "[session][algebra][coefficient]") {
    Session session;

    REQUIRE(session.execute({"Coefficient[3*x^2 + 2*x + 1, x]"}).output == "2");
    REQUIRE(session.execute({"Coefficient[3*x^2 + 2*x + 1, x, 2]"}).output == "3");
    REQUIRE(session.execute({"Coefficient[3*x^2 + 2*x + 1, x, 0]"}).output == "1");
    REQUIRE(session.execute({"CoefficientList[(1/2)*x^2 + x, x]"}).output == "{0, 1, 1/2}");

    const auto completion = session.execute({"Coeff", SessionOperation::complete});
    REQUIRE(completion.completions.size() == 2);
    REQUIRE(completion.completions.front().category == "pack");
    REQUIRE(completion.completions.front().owning_package == "core-algebra");

    const auto help = session.execute({"Coefficient", SessionOperation::help});
    REQUIRE(help.ok);
    REQUIRE(help.help_entries.size() == 1);
    REQUIRE(help.help_entries.front().owning_package == "core-algebra");
    REQUIRE_FALSE(help.help_entries.front().examples.empty());

    const auto bad_selector = session.execute({"Coefficient[x^2, {x}]"});
    REQUIRE_FALSE(bad_selector.ok);
    REQUIRE(bad_selector.diagnostics.size() == 1);
    REQUIRE(bad_selector.diagnostics.front().code == "kernel.invalid_form");

    const auto bad_exponent = session.execute({"Coefficient[x^2, x, -1]"});
    REQUIRE_FALSE(bad_exponent.ok);
    REQUIRE(bad_exponent.diagnostics.size() == 1);
    REQUIRE(bad_exponent.diagnostics.front().code == "kernel.invalid_form");

    const auto unsupported = session.execute({"Coefficient[y*x + x, x]"});
    REQUIRE_FALSE(unsupported.ok);
    REQUIRE(unsupported.diagnostics.size() == 1);
    REQUIRE(unsupported.diagnostics.front().code == "kernel.invalid_form");
}

TEST_CASE("Session exposes rational expression numerator and denominator", "[session][algebra][rational-expression]") {
    Session session;

    REQUIRE(session.execute({"Numerator[1/2]"}).output == "1");
    REQUIRE(session.execute({"Denominator[1/2]"}).output == "2");
    REQUIRE(session.execute({"Numerator[(1/2)*x]"}).output == "x");
    REQUIRE(session.execute({"Denominator[(1/2)*x]"}).output == "2");
    REQUIRE(session.execute({"Numerator[x/(x + 1)]"}).output == "x");
    REQUIRE(session.execute({"Denominator[x/(x + 1)]"}).output == "x + 1");

    const auto completion = session.execute({"Num", SessionOperation::complete});
    REQUIRE(completion.completions.size() == 1);
    REQUIRE(completion.completions.front().category == "pack");
    REQUIRE(completion.completions.front().owning_package == "core-algebra");

    const auto help = session.execute({"Denominator", SessionOperation::help});
    REQUIRE(help.ok);
    REQUIRE(help.help_entries.size() == 1);
    REQUIRE(help.help_entries.front().owning_package == "core-algebra");

    const auto unsupported = session.execute({"Numerator[0.5*x]"});
    REQUIRE_FALSE(unsupported.ok);
    REQUIRE(unsupported.diagnostics.size() == 1);
    REQUIRE(unsupported.diagnostics.front().code == "kernel.unsupported_construct");

    const auto zero_denominator = session.execute({"Denominator[x/0]"});
    REQUIRE_FALSE(zero_denominator.ok);
    REQUIRE(zero_denominator.diagnostics.size() == 1);
    REQUIRE(zero_denominator.diagnostics.front().code == "runtime.division_by_zero");
}

TEST_CASE("Session exposes rational expression transformations", "[session][algebra][rational-expression]") {
    Session session;

    REQUIRE(session.execute({"Together[1/x + 1/y]"}).output == "(x + y) / (x * y)");
    REQUIRE(session.execute({"Cancel[(x^2 - 1)/(x - 1)]"}).output == "x + 1");

    const auto completion = session.execute({"Tog", SessionOperation::complete});
    REQUIRE(completion.completions.size() == 1);
    REQUIRE(completion.completions.front().category == "pack");
    REQUIRE(completion.completions.front().owning_package == "core-algebra");

    const auto help = session.execute({"Cancel", SessionOperation::help});
    REQUIRE(help.ok);
    REQUIRE(help.help_entries.size() == 1);
    REQUIRE(help.help_entries.front().owning_package == "core-algebra");
    REQUIRE_FALSE(help.help_entries.front().examples.empty());

    const auto unsupported = session.execute({"Cancel[(x*y + x)/(x + 1)]"});
    REQUIRE_FALSE(unsupported.ok);
    REQUIRE(unsupported.diagnostics.size() == 1);
    REQUIRE(unsupported.diagnostics.front().code == "kernel.unsupported_construct");
}

TEST_CASE("Session preserves assumption contradiction diagnostics", "[session][assumptions][diagnostics]") {
    Session session;
    const auto result = session.execute({"Refine[x, And[x > 0, x <= 0]]"});
    REQUIRE_FALSE(result.ok);
    REQUIRE(result.diagnostics.size() == 1);
    REQUIRE(result.diagnostics.front().code == "runtime.assumption_contradiction");
}
