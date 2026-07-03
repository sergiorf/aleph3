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
    const auto failure = session.execute({"("});
    REQUIRE_FALSE(failure.ok);
    REQUIRE_FALSE(failure.diagnostics.empty());
    REQUIRE_FALSE(failure.diagnostics.front().code.empty());

    REQUIRE(session.execute({"0 + x", SessionOperation::simplify}).output == "x");
    REQUIRE(session.execute({"f[x]", SessionOperation::full_form}).output == "f[x]");
}
