#pragma once

#include "aleph_client/Protocol.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace aleph3::client {

enum class RuntimeLookupSource {
    explicit_path,
    environment,
    same_directory,
    path
};

struct RuntimeLookupResult {
    std::filesystem::path executable_path;
    RuntimeLookupSource source = RuntimeLookupSource::explicit_path;
};

struct RuntimeClientOptions {
    std::optional<std::filesystem::path> runtime_path;
    std::string runtime_name;
    std::vector<std::string> runtime_arguments;
    std::map<std::string, std::string> environment;
    std::string client_name = "aleph-client";
    std::string client_version = "0";
    int protocol_version = 1;
    std::chrono::milliseconds startup_timeout{5000};
    std::chrono::milliseconds request_timeout{5000};
    std::chrono::milliseconds shutdown_timeout{1000};
    std::size_t max_frame_bytes = 8u * 1024u * 1024u;
};

struct RuntimeClientErrorDetail {
    std::string code;
    std::string message;
    std::optional<ProtocolError> protocol_error;
};

class RuntimeClientException : public std::runtime_error {
public:
    RuntimeClientException(std::string code, std::string message);
    RuntimeClientException(std::string code, std::string message, ProtocolError protocol_error);

    [[nodiscard]] const std::string& code() const noexcept;
    [[nodiscard]] const std::optional<ProtocolError>& protocol_error() const noexcept;

private:
    std::string code_;
    std::optional<ProtocolError> protocol_error_;
};

[[nodiscard]] RuntimeLookupResult resolve_runtime(const RuntimeClientOptions& options);
[[nodiscard]] const char* to_string(RuntimeLookupSource source) noexcept;

class RuntimeClient {
public:
    explicit RuntimeClient(RuntimeClientOptions options = {});
    ~RuntimeClient();

    RuntimeClient(const RuntimeClient&) = delete;
    RuntimeClient& operator=(const RuntimeClient&) = delete;
    RuntimeClient(RuntimeClient&&) noexcept;
    RuntimeClient& operator=(RuntimeClient&&) noexcept;

    [[nodiscard]] const RuntimeLookupResult& lookup_result() const;
    [[nodiscard]] bool running() const noexcept;

    InitializeResult start();
    ProtocolResponse request(
        std::string_view method,
        const std::map<std::string, std::string>& string_params = {},
        std::chrono::milliseconds timeout = std::chrono::milliseconds{0});
    ProtocolResponse evaluate(std::string_view source);
    ProtocolResponse simplify(std::string_view source);
    ProtocolResponse full_form(std::string_view source);
    ProtocolResponse help(std::string_view query);
    ProtocolResponse complete(std::string_view query);
    ProtocolResponse packages();
    ProtocolResponse capabilities();
    ProtocolResponse version();
    ProtocolResponse reset();
    void shutdown();

private:
    struct Impl;

    RuntimeClientOptions options_;
    RuntimeLookupResult lookup_;
    std::unique_ptr<Impl> impl_;
    std::int64_t next_request_id_ = 1;

    [[nodiscard]] RequestId next_id();
    [[nodiscard]] ProtocolResponse send_payload(std::string payload, std::chrono::milliseconds timeout);
};

}  // namespace aleph3::client
