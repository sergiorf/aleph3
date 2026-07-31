#include "web/WebApi.hpp"

#include "json.hpp"
#include "notebook/Notebook.hpp"
#include "web/NotebookStore.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <chrono>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace aleph3::web {

namespace {

using Json = nlohmann::json;

std::string random_hex_id() {
    static thread_local std::mt19937_64 engine{std::random_device{}()};
    std::uniform_int_distribution<std::uint64_t> distribution;
    std::ostringstream out;
    out << std::hex;
    for (int i = 0; i < 2; ++i) {
        const auto value = distribution(engine);
        out.width(16);
        out.fill('0');
        out << value;
    }
    return out.str();
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string header_value(const ApiRequest& request, const std::string& name) {
    const auto wanted = lower_ascii(name);
    for (const auto& [key, value] : request.headers) {
        if (lower_ascii(key) == wanted) {
            return value;
        }
    }
    return {};
}

struct ParsedTarget {
    std::string path;
    std::map<std::string, std::string> query;
};

int hex_value(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
    if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
    return -1;
}

std::string url_decode(std::string_view value) {
    std::string decoded;
    decoded.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '+') {
            decoded.push_back(' ');
            continue;
        }
        if (value[i] == '%' && i + 2 < value.size()) {
            const int high = hex_value(value[i + 1]);
            const int low = hex_value(value[i + 2]);
            if (high >= 0 && low >= 0) {
                decoded.push_back(static_cast<char>((high << 4) | low));
                i += 2;
                continue;
            }
        }
        decoded.push_back(value[i]);
    }
    return decoded;
}

ParsedTarget parse_target(const std::string& target) {
    ParsedTarget parsed;
    const auto query_start = target.find('?');
    parsed.path = query_start == std::string::npos ? target : target.substr(0, query_start);
    if (query_start == std::string::npos) {
        return parsed;
    }

    std::size_t cursor = query_start + 1;
    while (cursor <= target.size()) {
        const auto next = target.find('&', cursor);
        const auto length = (next == std::string::npos ? target.size() : next) - cursor;
        const auto parameter = std::string_view(target).substr(cursor, length);
        if (!parameter.empty()) {
            const auto equals = parameter.find('=');
            const auto key = equals == std::string_view::npos ? parameter : parameter.substr(0, equals);
            const auto value = equals == std::string_view::npos ? std::string_view{} : parameter.substr(equals + 1);
            parsed.query[url_decode(key)] = url_decode(value);
        }
        if (next == std::string::npos) break;
        cursor = next + 1;
    }
    return parsed;
}

std::vector<std::string> split_path(const std::string& path) {
    std::vector<std::string> parts;
    std::string current;
    for (const char ch : path) {
        if (ch == '/') {
            if (!current.empty()) {
                parts.push_back(std::move(current));
                current.clear();
            }
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        parts.push_back(std::move(current));
    }
    return parts;
}

std::string severity_name(DiagnosticSeverity severity) {
    switch (severity) {
        case DiagnosticSeverity::error: return "error";
        case DiagnosticSeverity::warning: return "warning";
        case DiagnosticSeverity::note: return "note";
    }
    return "error";
}

Json diagnostic_json(const session::SessionDiagnostic& diagnostic) {
    Json encoded = {
        {"code", diagnostic.code},
        {"severity", severity_name(diagnostic.severity)},
        {"message", diagnostic.message}
    };
    if (!diagnostic.span.empty()) {
        encoded["span"] = {
            {"startOffset", diagnostic.span.start_offset},
            {"endOffset", diagnostic.span.end_offset},
            {"line", diagnostic.span.line},
            {"column", diagnostic.span.column}
        };
    }
    return encoded;
}

Json completion_json(const session::SessionCompletion& completion) {
    return {
        {"name", completion.name},
        {"category", completion.category},
        {"owningPackage", completion.owning_package.empty() ? Json(nullptr) : Json(completion.owning_package)},
        {"documentation", completion.documentation}
    };
}

Json help_entry_json(const session::SessionHelpEntry& entry) {
    return {
        {"name", entry.name},
        {"category", entry.category},
        {"owningPackage", entry.owning_package.empty() ? Json(nullptr) : Json(entry.owning_package)},
        {"description", entry.description},
        {"forms", entry.forms},
        {"examples", entry.examples},
        {"exactness", entry.exactness},
        {"unsupported", entry.unsupported},
        {"manualAnchor", entry.manual_anchor.empty() ? Json(nullptr) : Json(entry.manual_anchor)}
    };
}

Json document_json_from_bytes(const std::string& bytes) {
    return Json::parse(bytes);
}

Json notebook_summary_json(const StoredNotebook& notebook) {
    return {
        {"id", notebook.id},
        {"title", notebook.title},
        {"createdAt", notebook.created_at},
        {"updatedAt", notebook.updated_at},
        {"lastOpenedAt", notebook.last_opened_at},
        {"sizeBytes", notebook.size_bytes}
    };
}

Json notebook_detail_json(const StoredNotebook& notebook) {
    auto encoded = notebook_summary_json(notebook);
    encoded["document"] = document_json_from_bytes(notebook.document_json);
    return encoded;
}

ApiResponse json_response(int status, Json body) {
    ApiResponse response;
    response.status = status;
    response.headers["Content-Type"] = "application/json; charset=utf-8";
    response.body = body.dump();
    return response;
}

ApiResponse ok(Json body) {
    body["status"] = "ok";
    return json_response(200, std::move(body));
}

ApiResponse created(Json body) {
    body["status"] = "ok";
    return json_response(201, std::move(body));
}

ApiResponse no_content() {
    ApiResponse response;
    response.status = 204;
    return response;
}

ApiResponse error_response(int status, std::string code, std::string message) {
    return json_response(status, {
        {"status", "error"},
        {"error", {
            {"code", std::move(code)},
            {"message", std::move(message)}
        }}
    });
}

bool method_is(const ApiRequest& request, const char* method) {
    return request.method == method;
}

Json parse_body(const ApiRequest& request) {
    if (request.body.empty()) {
        return Json::object();
    }
    return Json::parse(request.body);
}

std::string timestamp_string() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

std::string title_from_body(const Json& body) {
    if (!body.contains("title")) {
        return "Untitled";
    }
    if (!body.at("title").is_string()) {
        throw std::invalid_argument("Notebook title must be a string.");
    }
    auto title = body.at("title").get<std::string>();
    if (title.empty()) {
        return "Untitled";
    }
    return title;
}

std::string notebook_json_from_body(const Json& body, const notebook::PersistenceLimits& limits) {
    if (!body.contains("document") && !body.contains("documentJson")) {
        return notebook::encode_document(notebook::Document{}, limits);
    }
    if (body.contains("document") && body.contains("documentJson")) {
        throw std::invalid_argument("Notebook requests may provide either `document` or `documentJson`, not both.");
    }

    std::string bytes;
    if (body.contains("documentJson")) {
        if (!body.at("documentJson").is_string()) {
            throw std::invalid_argument("Notebook `documentJson` must be a string.");
        }
        bytes = body.at("documentJson").get<std::string>();
    } else {
        bytes = body.at("document").dump();
    }
    const auto document = notebook::decode_document(bytes, limits);
    return notebook::encode_document(document, limits);
}

}  // namespace

WebApi::WebApi(ApiLimits limits, Clock clock, IdGenerator generate_id)
    : WebApi(std::move(limits), std::move(clock), std::move(generate_id), make_memory_notebook_store()) {
}

WebApi::WebApi(
    ApiLimits limits,
    Clock clock,
    IdGenerator generate_id,
    std::unique_ptr<NotebookStore> notebook_store)
    : limits_(limits),
      clock_(std::move(clock)),
      generate_id_(std::move(generate_id)),
      notebook_store_(std::move(notebook_store)) {
    if (!clock_) {
        clock_ = [] { return std::chrono::steady_clock::now(); };
    }
    if (!generate_id_) {
        generate_id_ = random_hex_id;
    }
    if (!notebook_store_) {
        throw std::invalid_argument("A notebook store is required.");
    }
}

WebApi::~WebApi() = default;

std::size_t WebApi::active_session_count() const noexcept {
    return sessions_.size();
}

void WebApi::expire_idle_sessions() {
    const auto now = clock_();
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (now - it->second.last_used_at > limits_.session_idle_ttl) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

ApiResponse WebApi::handle(const ApiRequest& request) {
    if (request.body.size() > limits_.max_request_body_bytes) {
        return error_response(413, "web.request_too_large", "The request body exceeds the configured limit.");
    }

    expire_idle_sessions();
    const auto target = parse_target(request.path);
    const auto parts = split_path(target.path);

    try {
        if (method_is(request, "GET") && parts == std::vector<std::string>{"api", "health"}) {
            return ok({{"service", "aleph3-web-api"}, {"ready", true}});
        }

        if (method_is(request, "POST") && parts == std::vector<std::string>{"api", "clients"}) {
            std::string client_id;
            do {
                client_id = generate_id_();
            } while (client_id.empty() || clients_.contains(client_id) || notebook_store_->client_exists(client_id));
            const auto now = clock_();
            clients_.emplace(client_id, ClientRecord{now});
            notebook_store_->create_client(client_id, timestamp_string());
            auto response = created({{"anonymousClientId", client_id}});
            response.headers["Set-Cookie"] = "aleph3_client=" + client_id + "; Path=/; SameSite=Lax";
            return response;
        }

        const auto client_id = header_value(request, "X-Aleph3-Client");
        if (client_id.empty()) {
            return error_response(401, "web.missing_client", "An anonymous client identifier is required.");
        }
        if (!clients_.contains(client_id) && !notebook_store_->client_exists(client_id)) {
            return error_response(403, "web.unknown_client", "The anonymous client identifier is not recognized.");
        }
        if (!clients_.contains(client_id)) {
            clients_.emplace(client_id, ClientRecord{clock_()});
        }

        if (method_is(request, "POST") && parts == std::vector<std::string>{"api", "sessions"}) {
            std::size_t owned_sessions = 0;
            for (const auto& [_, record] : sessions_) {
                if (record.client_id == client_id) {
                    ++owned_sessions;
                }
            }
            if (owned_sessions >= limits_.max_sessions_per_client) {
                return error_response(429, "web.session_quota_exceeded", "The anonymous client has too many active sessions.");
            }

            std::string session_id;
            do {
                session_id = generate_id_();
            } while (session_id.empty() || sessions_.contains(session_id) || clients_.contains(session_id));
            const auto now = clock_();
            sessions_.emplace(session_id, SessionRecord{
                client_id,
                std::make_unique<session::Session>(),
                now,
                now});
            return created({{"sessionId", session_id}});
        }

        if (parts.size() == 3 && parts[0] == "api" && parts[1] == "sessions") {
            auto session_it = sessions_.find(parts[2]);
            if (session_it == sessions_.end()) {
                return error_response(404, "web.unknown_session", "The session identifier is not recognized.");
            }
            auto& record = session_it->second;
            if (record.client_id != client_id) {
                return error_response(403, "web.session_forbidden", "The session belongs to a different anonymous client.");
            }

            if (method_is(request, "GET")) {
                record.last_used_at = clock_();
                return ok({{"sessionId", parts[2]}});
            }
            if (method_is(request, "DELETE")) {
                sessions_.erase(session_it);
                return no_content();
            }
        }

        if (parts.size() == 4 && parts[0] == "api" && parts[1] == "sessions") {
            auto session_it = sessions_.find(parts[2]);
            if (session_it == sessions_.end()) {
                return error_response(404, "web.unknown_session", "The session identifier is not recognized.");
            }
            auto& record = session_it->second;
            if (record.client_id != client_id) {
                return error_response(403, "web.session_forbidden", "The session belongs to a different anonymous client.");
            }

            if (method_is(request, "POST") && parts[3] == "reset") {
                record.session->reset();
                record.last_used_at = clock_();
                return ok({{"sessionId", parts[2]}, {"reset", true}});
            }

            if (method_is(request, "POST") && parts[3] == "evaluate") {
                const auto body = parse_body(request);
                if (!body.contains("source") || !body.at("source").is_string()) {
                    return error_response(400, "web.invalid_request", "Evaluation requests require a string `source` field.");
                }
                const auto source = body.at("source").get<std::string>();
                if (source.size() > limits_.max_evaluate_source_bytes) {
                    return error_response(413, "web.source_too_large", "The expression source exceeds the configured limit.");
                }

                const auto result = record.session->execute({source, session::SessionOperation::evaluate});
                record.last_used_at = clock_();
                Json diagnostics = Json::array();
                for (const auto& diagnostic : result.diagnostics) {
                    diagnostics.push_back(diagnostic_json(diagnostic));
                }
                return ok({
                    {"sessionId", parts[2]},
                    {"result", {
                        {"status", result.ok ? "ok" : "error"},
                        {"canonicalText", result.ok ? Json(result.output) : Json(nullptr)},
                        {"diagnostics", diagnostics}
                    }}
                });
            }

            if (method_is(request, "GET") && parts[3] == "complete") {
                const auto prefix_it = target.query.find("prefix");
                const auto prefix = prefix_it == target.query.end() ? std::string{} : prefix_it->second;
                const auto result = record.session->execute({prefix, session::SessionOperation::complete});
                record.last_used_at = clock_();
                if (!result.ok) {
                    return error_response(500, "web.completion_failed", "Completion lookup failed.");
                }

                Json completions = Json::array();
                for (const auto& completion : result.completions) {
                    completions.push_back(completion_json(completion));
                }
                return ok({
                    {"sessionId", parts[2]},
                    {"prefix", prefix},
                    {"completions", completions}
                });
            }

            if (method_is(request, "GET") && parts[3] == "help") {
                const auto query_it = target.query.find("query");
                const auto query = query_it == target.query.end() ? std::string{} : query_it->second;
                const auto result = record.session->execute({query, session::SessionOperation::help});
                record.last_used_at = clock_();
                if (!result.ok) {
                    return error_response(500, "web.help_failed", "Help lookup failed.");
                }
                if (!query.empty() && result.help_entries.empty()) {
                    return error_response(404, "web.help_not_found", "No supported help entry matches the query.");
                }

                Json entries = Json::array();
                for (const auto& entry : result.help_entries) {
                    entries.push_back(help_entry_json(entry));
                }
                return ok({
                    {"sessionId", parts[2]},
                    {"query", query},
                    {"entries", entries}
                });
            }
        }

        if (parts.size() == 2 && parts[0] == "api" && parts[1] == "notebooks") {
            if (method_is(request, "POST")) {
                const auto body = parse_body(request);
                const auto title = title_from_body(body);
                if (title.size() > limits_.max_notebook_title_bytes) {
                    return error_response(413, "web.notebook_title_too_large", "The notebook title exceeds the configured limit.");
                }

                notebook::PersistenceLimits notebook_limits;
                notebook_limits.max_file_bytes = limits_.max_notebook_document_bytes;
                const auto document_json = notebook_json_from_body(body, notebook_limits);
                if (document_json.size() > limits_.max_notebook_document_bytes) {
                    return error_response(413, "web.notebook_too_large", "The notebook document exceeds the configured limit.");
                }
                if (notebook_store_->notebook_count_for_client(client_id) >= limits_.max_notebooks_per_client) {
                    return error_response(429, "web.notebook_quota_exceeded", "The anonymous client has too many notebooks.");
                }
                const auto current_bytes = notebook_store_->stored_bytes_for_client(client_id);
                if (document_json.size() > limits_.max_stored_notebook_bytes_per_client ||
                    current_bytes > limits_.max_stored_notebook_bytes_per_client - document_json.size()) {
                    return error_response(429, "web.notebook_storage_quota_exceeded", "The anonymous client has exceeded notebook storage quota.");
                }

                std::string notebook_id;
                do {
                    notebook_id = generate_id_();
                } while (notebook_id.empty() || clients_.contains(notebook_id) || sessions_.contains(notebook_id) ||
                         notebook_store_->get_notebook(notebook_id).has_value());
                const auto now = timestamp_string();
                StoredNotebook notebook{
                    notebook_id,
                    client_id,
                    title,
                    document_json,
                    now,
                    now,
                    now,
                    document_json.size()};
                notebook_store_->create_notebook(notebook);
                return created({{"notebook", notebook_detail_json(notebook)}});
            }

            if (method_is(request, "GET")) {
                Json notebooks = Json::array();
                for (const auto& notebook : notebook_store_->list_notebooks(client_id)) {
                    notebooks.push_back(notebook_summary_json(notebook));
                }
                return ok({{"notebooks", notebooks}});
            }
        }

        if (parts.size() == 3 && parts[0] == "api" && parts[1] == "notebooks") {
            auto notebook = notebook_store_->get_notebook(parts[2]);
            if (!notebook) {
                return error_response(404, "web.unknown_notebook", "The notebook identifier is not recognized.");
            }
            if (notebook->anonymous_client_id != client_id) {
                return error_response(403, "web.notebook_forbidden", "The notebook belongs to a different anonymous client.");
            }

            if (method_is(request, "GET")) {
                const auto now = timestamp_string();
                notebook_store_->touch_notebook(parts[2], now);
                notebook->last_opened_at = now;
                return ok({{"notebook", notebook_detail_json(*notebook)}});
            }

            if (method_is(request, "PUT")) {
                const auto body = parse_body(request);
                const auto title = title_from_body(body);
                if (title.size() > limits_.max_notebook_title_bytes) {
                    return error_response(413, "web.notebook_title_too_large", "The notebook title exceeds the configured limit.");
                }
                notebook::PersistenceLimits notebook_limits;
                notebook_limits.max_file_bytes = limits_.max_notebook_document_bytes;
                const auto document_json = notebook_json_from_body(body, notebook_limits);
                if (document_json.size() > limits_.max_notebook_document_bytes) {
                    return error_response(413, "web.notebook_too_large", "The notebook document exceeds the configured limit.");
                }
                const auto current_bytes = notebook_store_->stored_bytes_for_client(client_id);
                const auto bytes_without_existing = current_bytes >= notebook->size_bytes ? current_bytes - notebook->size_bytes : 0;
                if (document_json.size() > limits_.max_stored_notebook_bytes_per_client ||
                    bytes_without_existing > limits_.max_stored_notebook_bytes_per_client - document_json.size()) {
                    return error_response(429, "web.notebook_storage_quota_exceeded", "The anonymous client has exceeded notebook storage quota.");
                }

                const auto now = timestamp_string();
                notebook_store_->update_notebook(parts[2], title, document_json, now, document_json.size());
                notebook->title = title;
                notebook->document_json = document_json;
                notebook->updated_at = now;
                notebook->size_bytes = document_json.size();
                return ok({{"notebook", notebook_detail_json(*notebook)}});
            }

            if (method_is(request, "DELETE")) {
                notebook_store_->delete_notebook(parts[2]);
                return no_content();
            }
        }
    } catch (const nlohmann::json::exception& error) {
        return error_response(400, "web.invalid_json", std::string("Invalid JSON request body: ") + error.what());
    } catch (const notebook::DocumentError& error) {
        if (error.code() == "notebook.limit_exceeded") {
            return error_response(413, error.code(), error.what());
        }
        return error_response(400, error.code(), error.what());
    } catch (const NotebookStoreError& error) {
        return error_response(500, error.code(), error.what());
    } catch (const std::invalid_argument& error) {
        return error_response(400, "web.invalid_request", error.what());
    } catch (const std::exception& error) {
        return error_response(500, "web.internal_error", error.what());
    }

    return error_response(404, "web.not_found", "The requested API endpoint is not available.");
}

}  // namespace aleph3::web
