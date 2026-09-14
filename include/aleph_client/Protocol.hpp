#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace aleph3::client {

struct RequestId {
    using Value = std::variant<std::monostate, std::int64_t, std::string>;

    Value value;

    [[nodiscard]] static RequestId none();
    [[nodiscard]] static RequestId number(std::int64_t id);
    [[nodiscard]] static RequestId string(std::string id);

    [[nodiscard]] bool has_value() const;
    [[nodiscard]] bool operator==(const RequestId& other) const = default;
};

struct SourceSpan {
    std::size_t line = 0;
    std::size_t column = 0;
    std::size_t length = 0;

    [[nodiscard]] bool operator==(const SourceSpan& other) const = default;
};

struct ProtocolDiagnostic {
    std::string code;
    std::string severity;
    std::string message;
    std::optional<SourceSpan> span;

    [[nodiscard]] bool operator==(const ProtocolDiagnostic& other) const = default;
};

struct ProtocolError {
    std::string code;
    std::string message;
    std::vector<ProtocolDiagnostic> diagnostics;

    [[nodiscard]] bool operator==(const ProtocolError& other) const = default;
};

struct EvaluationResult {
    bool ok = false;
    std::string source;
    std::map<std::string, std::string> representations;
    std::vector<ProtocolDiagnostic> diagnostics;

    [[nodiscard]] bool operator==(const EvaluationResult& other) const = default;
};

struct InitializeParams {
    std::string client;
    std::string client_version;
    int protocol_version = 1;

    [[nodiscard]] bool operator==(const InitializeParams& other) const = default;
};

struct InitializeResult {
    std::string kernel_version;
    int protocol_version = 1;
    std::map<std::string, bool> capabilities;

    [[nodiscard]] bool operator==(const InitializeResult& other) const = default;
};

struct HelpEntry {
    std::string name;
    std::string category;
    std::string owning_package;
    std::string description;
    std::vector<std::string> forms;
    std::vector<std::string> examples;

    [[nodiscard]] bool operator==(const HelpEntry& other) const = default;
};

struct CompletionEntry {
    std::string name;
    std::string category;
    std::string owning_package;
    std::string documentation;

    [[nodiscard]] bool operator==(const CompletionEntry& other) const = default;
};

struct PackageEntry {
    std::string name;
    std::string version;
    std::string description;

    [[nodiscard]] bool operator==(const PackageEntry& other) const = default;
};

struct ProtocolResponse {
    RequestId id;
    std::optional<EvaluationResult> evaluation;
    std::optional<InitializeResult> initialize;
    std::optional<ProtocolError> error;

    [[nodiscard]] bool ok() const;
};

class ProtocolException : public std::runtime_error {
public:
    ProtocolException(std::string code, std::string message);

    [[nodiscard]] const std::string& code() const noexcept;

private:
    std::string code_;
};

[[nodiscard]] std::string encode_initialize_request(const RequestId& id, const InitializeParams& params);
[[nodiscard]] std::string encode_evaluate_request(const RequestId& id, std::string_view source);
[[nodiscard]] ProtocolResponse decode_response(std::string_view payload);

}  // namespace aleph3::client
