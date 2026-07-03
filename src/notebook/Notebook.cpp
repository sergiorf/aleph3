#include "notebook/Notebook.hpp"
#include "json.hpp"

#include <chrono>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <utility>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

namespace aleph3::notebook {

namespace {

std::string product_version() {
#if defined(aleph3_VERSION)
    return aleph3_VERSION;
#else
    return "unknown";
#endif
}

[[noreturn]] void throw_limit(std::string message) {
    throw DocumentError("notebook.limit_exceeded", std::move(message));
}

void validate_limits(const Document& document, const PersistenceLimits& limits) {
    if (document.cells.size() > limits.max_cells) throw_limit("Notebook cell count exceeds the configured limit.");
    if (document.results.size() > limits.max_results) throw_limit("Notebook result count exceeds the configured limit.");
    std::size_t aggregate_source = 0;
    for (const auto& cell : document.cells) {
        if (cell.id.size() > limits.max_cell_id_bytes) throw_limit("Notebook cell identifier exceeds the configured limit.");
        if (cell.source.size() > limits.max_text_bytes) throw_limit("Notebook cell source exceeds the configured limit.");
        aggregate_source += cell.source.size();
        if (aggregate_source > limits.max_aggregate_source_bytes) throw_limit("Notebook source exceeds the aggregate configured limit.");
    }
    for (const auto& result : document.results) {
        if (result.source_cell_id.size() > limits.max_cell_id_bytes) throw_limit("Notebook result identifier exceeds the configured limit.");
        if (result.output.size() > limits.max_text_bytes) throw_limit("Notebook output exceeds the configured limit.");
        if (result.producer_version.size() > limits.max_text_bytes) throw_limit("Notebook producer version exceeds the configured limit.");
        if (result.diagnostics.size() > limits.max_diagnostics_per_result) throw_limit("Notebook diagnostic count exceeds the configured limit.");
        for (const auto& diagnostic : result.diagnostics) {
            if (diagnostic.code.size() > limits.max_text_bytes || diagnostic.message.size() > limits.max_text_bytes) {
                throw_limit("Notebook diagnostic text exceeds the configured limit.");
            }
        }
    }
}

std::string cell_kind_name(CellKind kind) {
    return kind == CellKind::input ? "input" : "text";
}

CellKind parse_cell_kind(const std::string& name) {
    if (name == "input") return CellKind::input;
    if (name == "text") return CellKind::text;
    throw DocumentError("notebook.unsupported_cell_kind", "Unsupported notebook cell kind `" + name + "`.");
}

void replace_file_atomically(const std::filesystem::path& temporary, const std::filesystem::path& destination) {
#if defined(_WIN32)
    const auto temporary_w = temporary.wstring();
    const auto destination_w = destination.wstring();
    BOOL replaced = FALSE;
    std::error_code exists_error;
    const bool destination_exists = std::filesystem::exists(destination, exists_error);
    if (exists_error) {
        throw DocumentError("notebook.atomic_replace_failed", "Unable to inspect notebook destination `" + destination.string() + "`: " + exists_error.message());
    }
    if (destination_exists) {
        replaced = ReplaceFileW(destination_w.c_str(), temporary_w.c_str(), nullptr, REPLACEFILE_WRITE_THROUGH, nullptr, nullptr);
    } else {
        replaced = MoveFileExW(temporary_w.c_str(), destination_w.c_str(), MOVEFILE_WRITE_THROUGH);
    }
    if (replaced == FALSE) {
        throw DocumentError("notebook.atomic_replace_failed", "Unable to atomically replace notebook `" + destination.string() + "`.");
    }
#else
    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    if (error) {
        throw DocumentError("notebook.atomic_replace_failed", "Unable to atomically replace notebook `" + destination.string() + "`: " + error.message());
    }
#endif
}

}  // namespace

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
    std::map<std::string, CellKind> cell_kinds;
    for (const auto& cell : cells) {
        if (cell.id.empty()) {
            throw DocumentError("notebook.empty_cell_id", "Cell identifiers must not be empty.");
        }
        if (!identifiers.insert(cell.id).second) {
            throw DocumentError("notebook.duplicate_cell_id", "Cell identifiers must be unique within a document.");
        }
        cell_kinds.emplace(cell.id, cell.kind);
    }

    std::set<std::string> result_identifiers;
    for (const auto& result : results) {
        const auto cell = cell_kinds.find(result.source_cell_id);
        if (cell == cell_kinds.end() || cell->second != CellKind::input) {
            throw DocumentError("notebook.invalid_result_reference", "Generated results must reference an input cell.");
        }
        if (!result_identifiers.insert(result.source_cell_id).second) {
            throw DocumentError("notebook.duplicate_result", "An input cell may have only one generated result.");
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
            std::move(result.diagnostics),
            product_version()});
    }
    document.results = std::move(next_results);
}

std::string encode_document(const Document& document, const PersistenceLimits& limits) {
    document.validate();
    validate_limits(document, limits);
    nlohmann::json root = {
        {"format", document.format},
        {"version", document.version},
        {"cells", nlohmann::json::array()}};
    for (const auto& cell : document.cells) {
        root["cells"].push_back({{"id", cell.id}, {"kind", cell_kind_name(cell.kind)}, {"source", cell.source}});
    }
    if (!document.results.empty()) {
        root["results"] = nlohmann::json::array();
        for (const auto& result : document.results) {
            nlohmann::json encoded_result = {
                {"source_cell_id", result.source_cell_id},
                {"ok", result.ok},
                {"output", result.output},
                {"diagnostics", nlohmann::json::array()},
                {"producer_version", result.producer_version}};
            for (const auto& diagnostic : result.diagnostics) {
                encoded_result["diagnostics"].push_back({{"code", diagnostic.code}, {"message", diagnostic.message}});
            }
            root["results"].push_back(std::move(encoded_result));
        }
    }
    std::string bytes;
    try {
        bytes = root.dump(2);
    } catch (const nlohmann::json::exception& error) {
        throw DocumentError("notebook.invalid_utf8", std::string("Notebook text is not valid UTF-8: ") + error.what());
    }
    if (bytes.size() > limits.max_file_bytes) throw_limit("Encoded notebook exceeds the configured file-size limit.");
    return bytes;
}

Document decode_document(std::string_view bytes, const PersistenceLimits& limits) {
    if (bytes.size() > limits.max_file_bytes) throw_limit("Notebook exceeds the configured file-size limit.");
    try {
        const auto root = nlohmann::json::parse(bytes);
        if (!root.is_object()) throw DocumentError("notebook.invalid_schema", "Notebook root must be an object.");
        Document document;
        document.format = root.at("format").get<std::string>();
        document.version = root.at("version").get<std::uint32_t>();
        document.validate();
        const auto& cells = root.at("cells");
        if (!cells.is_array()) throw DocumentError("notebook.invalid_schema", "Notebook cells must be an array.");
        if (cells.size() > limits.max_cells) throw_limit("Notebook cell count exceeds the configured limit.");
        for (const auto& encoded_cell : cells) {
            document.cells.push_back({
                encoded_cell.at("id").get<std::string>(),
                parse_cell_kind(encoded_cell.at("kind").get<std::string>()),
                encoded_cell.at("source").get<std::string>()});
        }
        if (root.contains("results")) {
            const auto& results = root.at("results");
            if (!results.is_array()) throw DocumentError("notebook.invalid_schema", "Notebook results must be an array.");
            if (results.size() > limits.max_results) throw_limit("Notebook result count exceeds the configured limit.");
            for (const auto& encoded_result : results) {
                GeneratedResult result;
                result.source_cell_id = encoded_result.at("source_cell_id").get<std::string>();
                result.ok = encoded_result.at("ok").get<bool>();
                result.output = encoded_result.at("output").get<std::string>();
                result.producer_version = encoded_result.at("producer_version").get<std::string>();
                const auto& diagnostics = encoded_result.at("diagnostics");
                if (!diagnostics.is_array()) throw DocumentError("notebook.invalid_schema", "Notebook diagnostics must be an array.");
                if (diagnostics.size() > limits.max_diagnostics_per_result) throw_limit("Notebook diagnostic count exceeds the configured limit.");
                for (const auto& encoded_diagnostic : diagnostics) {
                    result.diagnostics.push_back({
                        encoded_diagnostic.at("code").get<std::string>(),
                        encoded_diagnostic.at("message").get<std::string>()});
                }
                document.results.push_back(std::move(result));
            }
        }
        document.validate();
        validate_limits(document, limits);
        return document;
    } catch (const DocumentError&) {
        throw;
    } catch (const nlohmann::json::exception& error) {
        throw DocumentError("notebook.invalid_json", std::string("Invalid notebook JSON: ") + error.what());
    }
}

Document load_document(const std::filesystem::path& path, const PersistenceLimits& limits) {
    std::error_code size_error;
    const auto size = std::filesystem::file_size(path, size_error);
    if (size_error) throw DocumentError("notebook.read_failed", "Unable to inspect notebook `" + path.string() + "`: " + size_error.message());
    if (size > limits.max_file_bytes) throw_limit("Notebook exceeds the configured file-size limit.");
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw DocumentError("notebook.read_failed", "Unable to open notebook `" + path.string() + "`.");
    std::ostringstream contents;
    contents << stream.rdbuf();
    if (stream.bad()) throw DocumentError("notebook.read_failed", "Unable to read notebook `" + path.string() + "`.");
    return decode_document(contents.str(), limits);
}

void save_document_atomic(const Document& document, const std::filesystem::path& path, const PersistenceLimits& limits) {
    const auto bytes = encode_document(document, limits);
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto temporary = path.parent_path() / (path.filename().string() + ".tmp-" + std::to_string(nonce));
    try {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) throw DocumentError("notebook.write_failed", "Unable to create temporary notebook for `" + path.string() + "`.");
        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        stream.flush();
        if (!stream) throw DocumentError("notebook.write_failed", "Unable to write temporary notebook for `" + path.string() + "`.");
        stream.close();
        replace_file_atomically(temporary, path);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}

}  // namespace aleph3::notebook
