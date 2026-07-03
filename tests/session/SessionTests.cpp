#include "session/Session.hpp"

#include <catch2/catch_test_macros.hpp>

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
