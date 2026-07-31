#include "web/EngineApi.hpp"

#include "json.hpp"

#include <algorithm>
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

std::string path_without_query(const std::string& target) {
    const auto query_start = target.find('?');
    return query_start == std::string::npos ? target : target.substr(0, query_start);
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

EngineApi::EngineApi(EngineApiLimits limits, Clock clock, IdGenerator generate_id)
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

std::size_t EngineApi::active_session_count() const noexcept {
    return sessions_.size();
}

void EngineApi::expire_idle_sessions() {
    const auto now = clock_();
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (now - it->second.last_used_at > limits_.session_idle_ttl) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

ApiResponse EngineApi::handle(const ApiRequest& request) {
    if (request.body.size() > limits_.max_request_body_bytes) {
        return error_response(413, "engine.request_too_large", "The request body exceeds the configured limit.");
    }

    expire_idle_sessions();
    const auto parts = split_path(path_without_query(request.path));

    try {
        if (method_is(request, "GET") && parts == std::vector<std::string>{"internal", "health"}) {
            return ok({{"service", "aleph3-engine"}, {"ready", true}});
        }

        if (method_is(request, "POST") && parts == std::vector<std::string>{"internal", "sessions"}) {
            std::string session_id;
            do {
                session_id = generate_id_();
            } while (session_id.empty() || sessions_.contains(session_id));
            const auto now = clock_();
            sessions_.emplace(session_id, SessionRecord{std::make_unique<session::Session>(), now, now});
            return created({{"sessionId", session_id}});
        }

        if (parts.size() == 3 && parts[0] == "internal" && parts[1] == "sessions") {
            auto session_it = sessions_.find(parts[2]);
            if (session_it == sessions_.end()) {
                return error_response(404, "engine.unknown_session", "The session identifier is not recognized.");
            }
            auto& record = session_it->second;

            if (method_is(request, "GET")) {
                record.last_used_at = clock_();
                return ok({{"sessionId", parts[2]}});
            }
            if (method_is(request, "DELETE")) {
                sessions_.erase(session_it);
                return no_content();
            }
        }

        if (parts.size() == 4 && parts[0] == "internal" && parts[1] == "sessions") {
            auto session_it = sessions_.find(parts[2]);
            if (session_it == sessions_.end()) {
                return error_response(404, "engine.unknown_session", "The session identifier is not recognized.");
            }
            auto& record = session_it->second;

            if (method_is(request, "POST") && parts[3] == "reset") {
                record.session->reset();
                record.last_used_at = clock_();
                return ok({{"sessionId", parts[2]}, {"reset", true}});
            }

            if (method_is(request, "POST") && parts[3] == "evaluate") {
                const auto body = parse_body(request);
                if (!body.contains("source") || !body.at("source").is_string()) {
                    return error_response(400, "engine.invalid_request", "Evaluation requests require a string `source` field.");
                }
                const auto source = body.at("source").get<std::string>();
                if (source.size() > limits_.max_evaluate_source_bytes) {
                    return error_response(413, "engine.source_too_large", "The expression source exceeds the configured limit.");
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
        }
    } catch (const nlohmann::json::exception& error) {
        return error_response(400, "engine.invalid_json", std::string("Invalid JSON request body: ") + error.what());
    } catch (const std::invalid_argument& error) {
        return error_response(400, "engine.invalid_request", error.what());
    } catch (const std::exception& error) {
        return error_response(500, "engine.internal_error", error.what());
    }

    return error_response(404, "engine.not_found", "The requested internal engine endpoint is not available.");
}

}  // namespace aleph3::web
