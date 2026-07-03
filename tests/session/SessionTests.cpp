#include "session/Session.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

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
    REQUIRE(result.packs.size() == 1);
    REQUIRE(result.packs.front().name == "core-algebra");
    REQUIRE(result.packs.front().symbols ==
        std::vector<std::string>{"Collect", "Expand", "Factor", "GCD", "PolynomialQuotient"});
}

TEST_CASE("Session completes registry and session symbols deterministically", "[session][completion]") {
    Session session;
    REQUIRE(session.execute({"alpha = 2"}).ok);
    REQUIRE(session.execute({"Factor[x_] := x"}).ok);

    const auto factor = session.execute({"Fa", SessionOperation::complete});
    REQUIRE(factor.ok);
    REQUIRE(factor.completions.size() == 1);
    REQUIRE(factor.completions.front().name == "Factor");
    REQUIRE(factor.completions.front().category == "function");

    const auto packs = session.execute({"Pol", SessionOperation::complete});
    REQUIRE(packs.completions.size() == 1);
    REQUIRE(packs.completions.front().name == "PolynomialQuotient");
    REQUIRE(packs.completions.front().category == "pack");
    REQUIRE(packs.completions.front().owning_package == "core-algebra");

    const auto builtin = session.execute({"Abs", SessionOperation::complete});
    REQUIRE(builtin.completions.size() == 1);
    REQUIRE(builtin.completions.front().category == "builtin");

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

TEST_CASE("Session reports polynomial division by zero with a stable diagnostic", "[session][algebra][diagnostics]") {
    Session session;
    const auto result = session.execute({"PolynomialQuotient[x, 0, x]"});
    REQUIRE_FALSE(result.ok);
    REQUIRE(result.diagnostics.size() == 1);
    REQUIRE(result.diagnostics.front().code == "runtime.division_by_zero");
}
