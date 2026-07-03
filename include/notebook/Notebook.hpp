/* GUI-independent notebook document model and session-backed execution. */
#pragma once

#include "session/Session.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace aleph3::notebook {

enum class CellKind { input, text };

struct Cell {
    std::string id;
    CellKind kind = CellKind::input;
    std::string source;
};

struct GeneratedResult {
    std::string source_cell_id;
    bool ok = false;
    std::string output;
    std::vector<session::SessionDiagnostic> diagnostics;
    std::string producer_version;
};

struct PersistenceLimits {
    std::size_t max_file_bytes = 8u * 1024u * 1024u;
    std::size_t max_cells = 10000;
    std::size_t max_results = 10000;
    std::size_t max_cell_id_bytes = 256;
    std::size_t max_text_bytes = 1024u * 1024u;
    std::size_t max_aggregate_source_bytes = 8u * 1024u * 1024u;
    std::size_t max_diagnostics_per_result = 128;
};

class DocumentError : public std::runtime_error {
public:
    DocumentError(std::string code, std::string message);
    [[nodiscard]] const std::string& code() const noexcept;

private:
    std::string code_;
};

class Document {
public:
    static constexpr std::string_view format_name = "aleph3-notebook";
    static constexpr std::uint32_t current_version = 1;
    using CellIdGenerator = std::function<std::string()>;

    std::string format{format_name};
    std::uint32_t version = current_version;
    std::vector<Cell> cells;
    std::vector<GeneratedResult> results;

    Cell& append_cell(CellKind kind, std::string source, const CellIdGenerator& generate_id);
    void validate() const;
};

class Runner {
public:
    void run_all(Document& document) const;
};

[[nodiscard]] std::string encode_document(
    const Document& document,
    const PersistenceLimits& limits = {});
[[nodiscard]] Document decode_document(
    std::string_view bytes,
    const PersistenceLimits& limits = {});
[[nodiscard]] Document load_document(
    const std::filesystem::path& path,
    const PersistenceLimits& limits = {});
void save_document_atomic(
    const Document& document,
    const std::filesystem::path& path,
    const PersistenceLimits& limits = {});

}  // namespace aleph3::notebook
