/* Transport-independent web API core for the Aleph3 notebook MVP. */
#pragma once

#include "session/Session.hpp"

#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

namespace aleph3::web {

struct ApiRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct ApiResponse {
    int status = 500;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct ApiLimits {
    std::size_t max_request_body_bytes = 1024u * 1024u;
    std::size_t max_evaluate_source_bytes = 256u * 1024u;
    std::size_t max_sessions_per_client = 5;
    std::size_t max_notebook_document_bytes = 8u * 1024u * 1024u;
    std::size_t max_notebook_title_bytes = 256;
    std::size_t max_notebooks_per_client = 50;
    std::size_t max_stored_notebook_bytes_per_client = 32u * 1024u * 1024u;
    std::chrono::minutes session_idle_ttl{60};
};

class WebApi {
public:
    using Clock = std::function<std::chrono::steady_clock::time_point()>;
    using IdGenerator = std::function<std::string()>;

    explicit WebApi(
        ApiLimits limits = {},
        Clock clock = {},
        IdGenerator generate_id = {});
    WebApi(
        ApiLimits limits,
        Clock clock,
        IdGenerator generate_id,
        std::unique_ptr<class NotebookStore> notebook_store);
    ~WebApi();

    [[nodiscard]] ApiResponse handle(const ApiRequest& request);
    [[nodiscard]] std::size_t active_session_count() const noexcept;
    void expire_idle_sessions();

private:
    struct ClientRecord {
        std::chrono::steady_clock::time_point created_at;
    };

    struct SessionRecord {
        std::string client_id;
        std::unique_ptr<session::Session> session;
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point last_used_at;
    };

    ApiLimits limits_;
    Clock clock_;
    IdGenerator generate_id_;
    std::unique_ptr<class NotebookStore> notebook_store_;
    std::unordered_map<std::string, ClientRecord> clients_;
    std::unordered_map<std::string, SessionRecord> sessions_;
};

}  // namespace aleph3::web
