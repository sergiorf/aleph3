#include "aleph_client/Protocol.hpp"

#include "json.hpp"

#include <utility>

namespace aleph3::client {
namespace {
using Json = nlohmann::json;

Json id_to_json(const RequestId& id) {
    if (std::holds_alternative<std::monostate>(id.value)) return nullptr;
    if (const auto* number = std::get_if<std::int64_t>(&id.value)) return *number;
    return std::get<std::string>(id.value);
}

RequestId id_from_json(const Json& json) {
    if (json.is_null()) return RequestId::none();
    if (json.is_number_integer()) return RequestId::number(json.get<std::int64_t>());
    if (json.is_string()) return RequestId::string(json.get<std::string>());
    throw ProtocolException("protocol.invalid_id", "Protocol id must be null, an integer, or a string.");
}

void require_object(const Json& json, std::string_view code, std::string_view message) {
    if (!json.is_object()) {
        throw ProtocolException(std::string(code), std::string(message));
    }
}

std::string required_string(const Json& json, const char* field) {
    if (!json.contains(field) || !json.at(field).is_string()) {
        throw ProtocolException("protocol.missing_field", std::string("Protocol field `") + field + "` must be a string.");
    }
    return json.at(field).get<std::string>();
}

int required_int(const Json& json, const char* field) {
    if (!json.contains(field) || !json.at(field).is_number_integer()) {
        throw ProtocolException("protocol.missing_field", std::string("Protocol field `") + field + "` must be an integer.");
    }
    return json.at(field).get<int>();
}

ProtocolDiagnostic diagnostic_from_json(const Json& json) {
    require_object(json, "protocol.invalid_diagnostic", "Protocol diagnostics must be objects.");
    ProtocolDiagnostic diagnostic;
    diagnostic.code = required_string(json, "code");
    diagnostic.severity = json.value("severity", "");
    diagnostic.message = required_string(json, "message");
    if (json.contains("span") && !json.at("span").is_null()) {
        const auto& span = json.at("span");
        require_object(span, "protocol.invalid_span", "Protocol diagnostic span must be an object.");
        diagnostic.span = SourceSpan{
            span.value("line", 0u),
            span.value("column", 0u),
            span.value("length", 0u)};
    }
    return diagnostic;
}

std::vector<ProtocolDiagnostic> diagnostics_from_json(const Json& json) {
    if (json.is_null()) return {};
    if (!json.is_array()) {
        throw ProtocolException("protocol.invalid_diagnostics", "Protocol diagnostics must be an array.");
    }
    std::vector<ProtocolDiagnostic> diagnostics;
    diagnostics.reserve(json.size());
    for (const auto& item : json) {
        diagnostics.push_back(diagnostic_from_json(item));
    }
    return diagnostics;
}

EvaluationResult evaluation_from_json(const Json& json) {
    require_object(json, "protocol.invalid_result", "Evaluation result must be an object.");
    EvaluationResult result;
    if (!json.contains("ok") || !json.at("ok").is_boolean()) {
        throw ProtocolException("protocol.missing_field", "Evaluation result field `ok` must be a boolean.");
    }
    result.ok = json.at("ok").get<bool>();
    result.source = json.value("source", "");
    if (!json.contains("representations") || !json.at("representations").is_object()) {
        throw ProtocolException("protocol.missing_field", "Evaluation result field `representations` must be an object.");
    }
    for (const auto& [key, value] : json.at("representations").items()) {
        if (!value.is_string()) {
            throw ProtocolException("protocol.invalid_representation", "Evaluation representations must contain string values.");
        }
        result.representations.emplace(key, value.get<std::string>());
    }
    result.diagnostics = diagnostics_from_json(json.value("diagnostics", Json::array()));
    return result;
}

InitializeResult initialize_from_json(const Json& json) {
    require_object(json, "protocol.invalid_result", "Initialize result must be an object.");
    InitializeResult result;
    result.kernel_version = required_string(json, "kernelVersion");
    result.protocol_version = required_int(json, "protocolVersion");
    if (!json.contains("capabilities") || !json.at("capabilities").is_object()) {
        throw ProtocolException("protocol.missing_field", "Initialize result field `capabilities` must be an object.");
    }
    for (const auto& [key, value] : json.at("capabilities").items()) {
        if (!value.is_boolean()) {
            throw ProtocolException("protocol.invalid_capability", "Capability values must be booleans.");
        }
        result.capabilities.emplace(key, value.get<bool>());
    }
    return result;
}

ProtocolError error_from_json(const Json& json) {
    require_object(json, "protocol.invalid_error", "Protocol error must be an object.");
    ProtocolError error;
    error.code = required_string(json, "code");
    error.message = required_string(json, "message");
    error.diagnostics = diagnostics_from_json(json.value("diagnostics", Json::array()));
    return error;
}
}  // namespace

RequestId RequestId::none() {
    return {};
}

RequestId RequestId::number(std::int64_t id) {
    return RequestId{id};
}

RequestId RequestId::string(std::string id) {
    return RequestId{std::move(id)};
}

bool RequestId::has_value() const {
    return !std::holds_alternative<std::monostate>(value);
}

bool ProtocolResponse::ok() const {
    return !error.has_value();
}

ProtocolException::ProtocolException(std::string code, std::string message)
    : std::runtime_error(message), code_(std::move(code)) {}

const std::string& ProtocolException::code() const noexcept {
    return code_;
}

std::string encode_initialize_request(const RequestId& id, const InitializeParams& params) {
    const Json payload = {
        {"jsonrpc", "2.0"},
        {"id", id_to_json(id)},
        {"method", "initialize"},
        {"params",
         {
             {"client", params.client},
             {"clientVersion", params.client_version},
             {"protocolVersion", params.protocol_version},
         }}};
    return payload.dump();
}

std::string encode_evaluate_request(const RequestId& id, std::string_view source) {
    const Json payload = {
        {"jsonrpc", "2.0"},
        {"id", id_to_json(id)},
        {"method", "evaluate"},
        {"params", {{"source", std::string(source)}}}};
    return payload.dump();
}

ProtocolResponse decode_response(std::string_view payload) {
    Json root;
    try {
        root = Json::parse(payload.begin(), payload.end());
    } catch (const Json::exception& error) {
        throw ProtocolException("protocol.invalid_json", error.what());
    }

    require_object(root, "protocol.invalid_envelope", "Protocol response must be an object.");
    if (!root.contains("jsonrpc") || root.at("jsonrpc") != "2.0") {
        throw ProtocolException("protocol.invalid_version", "Protocol response must declare jsonrpc version 2.0.");
    }
    if (!root.contains("id")) {
        throw ProtocolException("protocol.missing_field", "Protocol response is missing `id`.");
    }

    ProtocolResponse response;
    response.id = id_from_json(root.at("id"));
    const auto has_result = root.contains("result");
    const auto has_error = root.contains("error");
    if (has_result == has_error) {
        throw ProtocolException("protocol.invalid_envelope", "Protocol response must contain exactly one of `result` or `error`.");
    }
    if (has_error) {
        response.error = error_from_json(root.at("error"));
        return response;
    }

    const auto& result = root.at("result");
    require_object(result, "protocol.invalid_result", "Protocol result must be an object.");
    if (result.contains("representations")) {
        response.evaluation = evaluation_from_json(result);
    } else if (result.contains("kernelVersion")) {
        response.initialize = initialize_from_json(result);
    } else {
        throw ProtocolException("protocol.unknown_result", "Protocol result shape is not recognized.");
    }
    return response;
}

}  // namespace aleph3::client
