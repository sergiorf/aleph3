#include "notebook/Notebook.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <filesystem>
#include <fstream>
#include <utility>
#include <vector>

using aleph3::notebook::Cell;
using aleph3::notebook::CellKind;
using aleph3::notebook::Document;
using aleph3::notebook::DocumentError;
using aleph3::notebook::Runner;

namespace {
Document make_document(std::vector<Cell> cells) {
    Document document;
    document.cells = std::move(cells);
    return document;
}
}  // namespace

TEST_CASE("Notebook documents preserve cell order and create stable identifiers", "[notebook][model]") {
    Document document;
    std::size_t next_id = 1;
    auto generate_id = [&] { return "cell-" + std::to_string(next_id++); };

    const auto& text = document.append_cell(CellKind::text, "A short derivation", generate_id);
    REQUIRE(text.id == "cell-1");
    const auto& input = document.append_cell(CellKind::input, "1/2 + 1/3", generate_id);

    REQUIRE(input.id == "cell-2");
    REQUIRE(document.cells[0].kind == CellKind::text);
    REQUIRE(document.cells[1].source == "1/2 + 1/3");
    REQUIRE_NOTHROW(document.validate());
}

TEST_CASE("Notebook documents reject invalid identity and version fields", "[notebook][model]") {
    auto empty_id = make_document({{"", CellKind::input, "1 + 1"}});
    REQUIRE_THROWS_AS(empty_id.validate(), DocumentError);

    auto duplicate = make_document({
        {"same", CellKind::input, "a = 2"},
        {"same", CellKind::text, "duplicate"}});
    REQUIRE_THROWS_AS(duplicate.validate(), DocumentError);

    auto wrong_format = make_document({{"one", CellKind::input, "1"}});
    wrong_format.format = "another-format";
    REQUIRE_THROWS_AS(wrong_format.validate(), DocumentError);

    auto future_version = make_document({{"one", CellKind::input, "1"}});
    future_version.version = Document::current_version + 1;
    REQUIRE_THROWS_AS(future_version.validate(), DocumentError);
}

TEST_CASE("Notebook Run All preserves definitions and skips text cells", "[notebook][runner]") {
    auto document = make_document({
        {"intro", CellKind::text, "Definitions flow downward."},
        {"define", CellKind::input, "a = 2"},
        {"use", CellKind::input, "a + 3"}});

    Runner{}.run_all(document);

    REQUIRE(document.results.size() == 2);
    REQUIRE(document.results[0].source_cell_id == "define");
    REQUIRE(document.results[0].ok);
    REQUIRE(document.results[1].source_cell_id == "use");
    REQUIRE(document.results[1].output == "5");
}

TEST_CASE("Notebook Run All starts clean and replaces generated results", "[notebook][runner]") {
    auto document = make_document({
        {"define", CellKind::input, "a = 2"},
        {"use", CellKind::input, "a + 3"}});
    Runner runner;
    runner.run_all(document);
    REQUIRE(document.results.back().output == "5");

    document.cells[0].source = "b = 10";
    runner.run_all(document);

    REQUIRE(document.results.size() == 2);
    REQUIRE(document.results[1].output == "a + 3");
}

TEST_CASE("Notebook runners isolate documents", "[notebook][runner]") {
    auto left = make_document({{"define", CellKind::input, "privateValue = 7"}});
    auto right = make_document({{"read", CellKind::input, "privateValue"}});
    Runner runner;

    runner.run_all(left);
    runner.run_all(right);

    REQUIRE(right.results.size() == 1);
    REQUIRE(right.results[0].output == "privateValue");
}

TEST_CASE("Notebook Run All records failures and continues", "[notebook][runner]") {
    auto document = make_document({
        {"invalid", CellKind::input, "("},
        {"valid", CellKind::input, "1 + 1"}});

    Runner{}.run_all(document);

    REQUIRE(document.results.size() == 2);
    REQUIRE_FALSE(document.results[0].ok);
    REQUIRE(document.results[0].diagnostics.size() == 1);
    REQUIRE(document.results[0].diagnostics[0].code == "session.parse_error");
    REQUIRE(document.results[1].ok);
    REQUIRE(document.results[1].output == "2");
}

TEST_CASE("Notebook representative fixture uses shared kernel and pack semantics", "[notebook][fixture]") {
    auto document = make_document({
        {"exact", CellKind::input, "1/2 + 1/3"},
        {"assign", CellKind::input, "x = 3"},
        {"state", CellKind::input, "x^2"},
        {"assumption", CellKind::input, "Refine[Sqrt[y^2], y >= 0]"},
        {"algebra", CellKind::input, "Expand[(z + 1) * (z + 2)]"},
        {"failure", CellKind::input, "PolynomialQuotient[z, 0, z]"}});

    Runner{}.run_all(document);

    REQUIRE(document.results.size() == 6);
    REQUIRE(document.results[0].output == "5/6");
    REQUIRE(document.results[2].output == "9");
    REQUIRE(document.results[3].output == "y");
    REQUIRE(document.results[4].ok);
    REQUIRE_FALSE(document.results[5].ok);
    REQUIRE(document.results[5].diagnostics[0].code == "runtime.division_by_zero");
}

TEST_CASE("Notebook JSON persistence round trips cells and cached diagnostics", "[notebook][persistence]") {
    auto document = make_document({
        {"bad", CellKind::input, "Refine[x, And[x > 0, x <= 0]]"},
        {"notes", CellKind::text, "Contradiction example"}});
    Runner{}.run_all(document);

    const auto encoded = aleph3::notebook::encode_document(document);
    const auto decoded = aleph3::notebook::decode_document(encoded);

    REQUIRE(decoded.cells.size() == 2);
    REQUIRE(decoded.cells[0].id == "bad");
    REQUIRE(decoded.results.size() == 1);
    REQUIRE(decoded.results[0].diagnostics[0].code == "runtime.assumption_contradiction");
    REQUIRE_FALSE(decoded.results[0].producer_version.empty());
}

TEST_CASE("Notebook JSON loading rejects corrupt, incompatible, and invalid documents", "[notebook][persistence]") {
    REQUIRE_THROWS_AS(aleph3::notebook::decode_document("{"), DocumentError);
    REQUIRE_THROWS_AS(
        aleph3::notebook::decode_document(
            R"({"format":"aleph3-notebook","version":2,"cells":[]})"),
        DocumentError);
    REQUIRE_THROWS_AS(
        aleph3::notebook::decode_document(
            R"({"format":"aleph3-notebook","version":1,"cells":[{"id":"x","kind":"future","source":"1"}]})"),
        DocumentError);
    REQUIRE_THROWS_AS(
        aleph3::notebook::decode_document(
            R"({"format":"aleph3-notebook","version":1,"cells":[],"results":[{"source_cell_id":"missing","ok":true,"output":"1","diagnostics":[],"producer_version":"0.1.0"}]})"),
        DocumentError);
}

TEST_CASE("Notebook JSON loading tolerates unknown optional fields", "[notebook][persistence][compatibility]") {
    const auto document = aleph3::notebook::decode_document(
        R"({"format":"aleph3-notebook","version":1,"future_metadata":{"theme":"dark"},"cells":[{"id":"one","kind":"input","source":"1","future_cell_field":true}]})");
    REQUIRE(document.cells.size() == 1);
    REQUIRE(document.cells[0].id == "one");
}

TEST_CASE("Notebook persistence enforces configured limits", "[notebook][persistence][limits]") {
    aleph3::notebook::PersistenceLimits limits;
    limits.max_cells = 1;
    auto document = make_document({
        {"one", CellKind::input, "1"},
        {"two", CellKind::input, "2"}});
    REQUIRE_THROWS_AS(aleph3::notebook::encode_document(document, limits), DocumentError);

    limits = {};
    limits.max_file_bytes = 8;
    REQUIRE_THROWS_AS(
        aleph3::notebook::decode_document(
            R"({"format":"aleph3-notebook","version":1,"cells":[]})", limits),
        DocumentError);
}

TEST_CASE("Notebook atomic save reloads and preserves an existing file on validation failure", "[notebook][persistence][filesystem]") {
    const auto path = std::filesystem::temp_directory_path() / "aleph3-notebook-persistence-test.json";
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); }
    } cleanup{path};

    auto document = make_document({{"one", CellKind::input, "1 + 1"}});
    aleph3::notebook::save_document_atomic(document, path);
    REQUIRE(aleph3::notebook::load_document(path).cells[0].source == "1 + 1");

    document.cells.push_back({"one", CellKind::text, "duplicate"});
    REQUIRE_THROWS_AS(aleph3::notebook::save_document_atomic(document, path), DocumentError);
    REQUIRE(aleph3::notebook::load_document(path).cells.size() == 1);
}
