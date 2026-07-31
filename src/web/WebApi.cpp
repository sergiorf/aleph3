#include "web/WebApi.hpp"

#include "json.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
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

}  // namespace

WebApi::WebApi(ApiLimits limits, Clock clock, IdGenerator generate_id)
    : limits_(limits),
      clock_(std::move(clock)),
      generate_id_(std::move(generate_id)) {
    if (!clock_) {
        clock_ = [] { return std::chrono::steady_clock::now(); };
    }
    if (!generate_id_) {
        generate_id_ = random_hex_id;
    }
}

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
            } while (client_id.empty() || clients_.contains(client_id));
            const auto now = clock_();
            clients_.emplace(client_id, ClientRecord{now});
            auto response = created({{"anonymousClientId", client_id}});
            response.headers["Set-Cookie"] = "aleph3_client=" + client_id + "; Path=/; SameSite=Lax";
            return response;
        }

        const auto client_id = header_value(request, "X-Aleph3-Client");
        if (client_id.empty()) {
            return error_response(401, "web.missing_client", "An anonymous client identifier is required.");
        }
        if (!clients_.contains(client_id)) {
            return error_response(403, "web.unknown_client", "The anonymous client identifier is not recognized.");
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
    } catch (const nlohmann::json::exception& error) {
        return error_response(400, "web.invalid_json", std::string("Invalid JSON request body: ") + error.what());
    } catch (const std::exception& error) {
        return error_response(500, "web.internal_error", error.what());
    }

    return error_response(404, "web.not_found", "The requested API endpoint is not available.");
}

}  // namespace aleph3::web
