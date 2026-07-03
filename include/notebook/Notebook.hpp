/* GUI-independent notebook document model and session-backed execution. */
#pragma once

#include "session/Session.hpp"

#include <cstdint>
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

}  // namespace aleph3::notebook
