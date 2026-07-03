#include "notebook/Notebook.hpp"

#include <set>
#include <utility>

namespace aleph3::notebook {

DocumentError::DocumentError(std::string code, std::string message)
    : std::runtime_error(std::move(message)), code_(std::move(code)) {
}

const std::string& DocumentError::code() const noexcept {
    return code_;
}

Cell& Document::append_cell(
    CellKind kind,
    std::string source,
    const CellIdGenerator& generate_id) {
    if (!generate_id) {
        throw DocumentError("notebook.missing_id_generator", "A cell identifier generator is required.");
    }
    auto id = generate_id();
    if (id.empty()) {
        throw DocumentError("notebook.empty_cell_id", "Cell identifiers must not be empty.");
    }
    for (const auto& cell : cells) {
        if (cell.id == id) {
            throw DocumentError("notebook.duplicate_cell_id", "Cell identifiers must be unique within a document.");
        }
    }
    cells.push_back({std::move(id), kind, std::move(source)});
    return cells.back();
}

void Document::validate() const {
    if (format != format_name) {
        throw DocumentError("notebook.unsupported_format", "The notebook format identifier is not supported.");
    }
    if (version != current_version) {
        throw DocumentError("notebook.unsupported_version", "The notebook document version is not supported.");
    }

    std::set<std::string> identifiers;
    for (const auto& cell : cells) {
        if (cell.id.empty()) {
            throw DocumentError("notebook.empty_cell_id", "Cell identifiers must not be empty.");
        }
        if (!identifiers.insert(cell.id).second) {
            throw DocumentError("notebook.duplicate_cell_id", "Cell identifiers must be unique within a document.");
        }
    }
}

void Runner::run_all(Document& document) const {
    document.validate();

    session::Session session;
    std::vector<GeneratedResult> next_results;
    for (const auto& cell : document.cells) {
        if (cell.kind == CellKind::text) {
            continue;
        }
        auto result = session.execute({cell.source, session::SessionOperation::evaluate});
        next_results.push_back({
            cell.id,
            result.ok,
            std::move(result.output),
            std::move(result.diagnostics)});
    }
    document.results = std::move(next_results);
}

}  // namespace aleph3::notebook
