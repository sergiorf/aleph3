#include "tooling/RuntimeProtocolServer.hpp"

#include "json.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace aleph3::tooling {
namespace {
using Json = nlohmann::json;

constexpr int protocol_version = 1;
constexpr std::string_view server_name = "aleph-runtime";

Json id_to_json(const Json& request) {
    if (!request.contains("id")) return nullptr;
    const auto& id = request.at("id");
    if (id.is_null() || id.is_number_integer() || id.is_string()) return id;
    return nullptr;
}

Json diagnostic_to_json(const session::SessionDiagnostic& diagnostic) {
    Json result = {
        {"code", diagnostic.code},
        {"severity", diagnostic.severity == DiagnosticSeverity::warning ? "warning" :
             diagnostic.severity == DiagnosticSeverity::note ? "note" : "error"},
        {"message", diagnostic.message},
    };
    if (!diagnostic.span.empty()) {
        result["span"] = {
            {"line", diagnostic.span.line},
            {"column", diagnostic.span.column},
            {"length", diagnostic.span.end_offset > diagnostic.span.start_offset
                ? diagnostic.span.end_offset - diagnostic.span.start_offset
                : 0u}};
    }
    return result;
}

Json error_body(std::string_view code, std::string_view message, Json diagnostics = Json::array()) {
    return {
        {"code", std::string(code)},
        {"message", std::string(message)},
        {"diagnostics", std::move(diagnostics)}};
}

Json response_with_result(const Json& id, Json result) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", std::move(result)}};
}

Json response_with_error(const Json& id, std::string_view code, std::string_view message, Json diagnostics = Json::array()) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", error_body(code, message, std::move(diagnostics))}};
}

std::vector<std::string> methods() {
    return {
        "initialize",
        "version",
        "capabilities",
        "evaluate",
        "simplify",
        "fullForm",
        "help",
        "complete",
        "packages",
        "reset",
        "shutdown"};
}

Json capability_flags() {
    Json result = Json::object();
    for (const auto& method : methods()) {
        result[method] = true;
    }
    return result;
}

Json capabilities_result() {
    return {
        {"methods", methods()},
        {"representations", std::vector<std::string>{"text/plain", "text/x-aleph-source"}},
        {"features",
         {
             {"singleSession", true},
             {"serialRequests", true},
             {"sessionReset", true},
             {"shutdown", true},
         }}};
}

Json version_result() {
    return {
        {"serverName", std::string(server_name)},
        {"kernelVersion", aleph3_VERSION},
        {"protocolVersion", protocol_version}};
}

std::string required_string_param(const Json& params, const char* field) {
    if (!params.is_object() || !params.contains(field) || !params.at(field).is_string()) {
        throw std::invalid_argument(std::string("Protocol params field `") + field + "` must be a string.");
    }
    return params.at(field).get<std::string>();
}

Json evaluation_result(const std::string& source, const session::SessionResult& session_result) {
    Json diagnostics = Json::array();
    for (const auto& diagnostic : session_result.diagnostics) {
        diagnostics.push_back(diagnostic_to_json(diagnostic));
    }
    return {
        {"ok", session_result.ok},
        {"source", source},
        {"representations",
         {
             {"text/plain", session_result.output},
             {"text/x-aleph-source", session_result.output},
         }},
        {"diagnostics", std::move(diagnostics)}};
}

Json first_session_error(const session::SessionResult& result) {
    Json diagnostics = Json::array();
    for (const auto& diagnostic : result.diagnostics) {
        diagnostics.push_back(diagnostic_to_json(diagnostic));
    }
    if (!result.diagnostics.empty()) {
        return error_body(result.diagnostics.front().code, result.diagnostics.front().message, std::move(diagnostics));
    }
    return error_body("session.failed", "Session operation failed.", std::move(diagnostics));
}

Json help_result(const session::SessionResult& session_result) {
    Json entries = Json::array();
    for (const auto& entry : session_result.help_entries) {
        entries.push_back({
            {"name", entry.name},
            {"category", entry.category},
            {"owningPackage", entry.owning_package},
            {"description", entry.description},
            {"forms", entry.forms},
            {"examples", entry.examples},
            {"exactness", entry.exactness},
            {"unsupported", entry.unsupported},
            {"manualAnchor", entry.manual_anchor},
        });
    }
    return {{"help", std::move(entries)}};
}

Json completion_result(const session::SessionResult& session_result) {
    Json entries = Json::array();
    for (const auto& entry : session_result.completions) {
        entries.push_back({
            {"name", entry.name},
            {"category", entry.category},
            {"owningPackage", entry.owning_package},
            {"documentation", entry.documentation},
        });
    }
    return {{"completions", std::move(entries)}};
}

Json packages_result(const session::SessionResult& session_result) {
    Json entries = Json::array();
    for (const auto& entry : session_result.packs) {
        entries.push_back({
            {"name", entry.name},
            {"version", ""},
            {"description", ""},
            {"symbols", entry.symbols},
        });
    }
    return {{"packages", std::move(entries)}};
}

session::SessionOperation operation_for_method(const std::string& method) {
    if (method == "evaluate") return session::SessionOperation::evaluate;
    if (method == "simplify") return session::SessionOperation::simplify;
    if (method == "fullForm") return session::SessionOperation::full_form;
    if (method == "help") return session::SessionOperation::help;
    if (method == "complete") return session::SessionOperation::complete;
    if (method == "packages") return session::SessionOperation::discover_packs;
    throw std::invalid_argument("Unsupported session method.");
}

bool method_takes_query(const std::string& method) {
    return method == "help" || method == "complete";
}

}  // namespace

std::string protocol_error_response(std::string_view code, std::string_view message) {
    return response_with_error(nullptr, code, message).dump();
}

std::string RuntimeProtocolServer::handle_payload(std::string_view payload) {
    Json request;
    try {
        request = Json::parse(payload.begin(), payload.end());
    } catch (const Json::exception& error) {
        return response_with_error(nullptr, "protocol.invalid_json", error.what()).dump();
    }

    const auto id = id_to_json(request);
    try {
        if (!request.is_object()) {
            return response_with_error(id, "protocol.invalid_envelope", "Protocol request must be an object.").dump();
        }
        if (!request.contains("jsonrpc") || request.at("jsonrpc") != "2.0") {
            return response_with_error(id, "protocol.invalid_version", "Protocol request must declare jsonrpc version 2.0.").dump();
        }
        if (!request.contains("method") || !request.at("method").is_string()) {
            return response_with_error(id, "protocol.missing_field", "Protocol request field `method` must be a string.").dump();
        }

        const auto method = request.at("method").get<std::string>();
        const auto params = request.value("params", Json::object());

        if (method == "initialize") {
            if (!params.is_object() || !params.contains("protocolVersion") || !params.at("protocolVersion").is_number_integer()) {
                return response_with_error(id, "protocol.missing_field", "Initialize params field `protocolVersion` must be an integer.").dump();
            }
            if (params.at("protocolVersion").get<int>() != protocol_version) {
                return response_with_error(id, "protocol.incompatible_version", "Unsupported Aleph runtime protocol version.").dump();
            }
            auto result = version_result();
            result["capabilities"] = capability_flags();
            return response_with_result(id, std::move(result)).dump();
        }
        if (method == "version") {
            return response_with_result(id, version_result()).dump();
        }
        if (method == "capabilities") {
            return response_with_result(id, capabilities_result()).dump();
        }
        if (method == "reset") {
            session_.reset();
            return response_with_result(id, Json{{"ok", true}, {"message", "session reset"}}).dump();
        }
        if (method == "shutdown") {
            shutdown_requested_ = true;
            return response_with_result(id, Json{{"ok", true}, {"message", "shutdown"}}).dump();
        }

        const auto supported_methods = methods();
        if (std::ranges::find(supported_methods, method) == supported_methods.end()) {
            return response_with_error(id, "protocol.unknown_method", "Unsupported protocol method.").dump();
        }

        std::string source;
        if (method == "packages") {
            if (!params.is_object() && !params.is_null()) {
                return response_with_error(id, "protocol.invalid_params", "Protocol params must be an object.").dump();
            }
        } else {
            source = required_string_param(params, method_takes_query(method) ? "query" : "source");
        }

        const auto operation = operation_for_method(method);
        const auto session_result = session_.execute({source, operation});
        if (!session_result.ok) {
            auto error = first_session_error(session_result);
            return Json{{"jsonrpc", "2.0"}, {"id", id}, {"error", std::move(error)}}.dump();
        }

        if (method == "help") return response_with_result(id, help_result(session_result)).dump();
        if (method == "complete") return response_with_result(id, completion_result(session_result)).dump();
        if (method == "packages") return response_with_result(id, packages_result(session_result)).dump();
        return response_with_result(id, evaluation_result(source, session_result)).dump();
    } catch (const std::invalid_argument& error) {
        return response_with_error(id, "protocol.invalid_params", error.what()).dump();
    } catch (const Json::exception& error) {
        return response_with_error(id, "protocol.invalid_envelope", error.what()).dump();
    } catch (const std::exception& error) {
        return response_with_error(id, "runtime.internal_error", error.what()).dump();
    }
}

bool RuntimeProtocolServer::shutdown_requested() const noexcept {
    return shutdown_requested_;
}

}  // namespace aleph3::tooling
