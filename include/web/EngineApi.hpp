/* Internal computation API for the Aleph3 web MVP engine service. */
#pragma once

#include "session/Session.hpp"
#include "web/WebApi.hpp"

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace aleph3::web {

struct EngineApiLimits {
    std::size_t max_request_body_bytes = 1024u * 1024u;
    std::size_t max_evaluate_source_bytes = 256u * 1024u;
    std::chrono::minutes session_idle_ttl{60};
};

class EngineApi {
public:
    using Clock = std::function<std::chrono::steady_clock::time_point()>;
    using IdGenerator = std::function<std::string()>;

    explicit EngineApi(
        EngineApiLimits limits = {},
        Clock clock = {},
        IdGenerator generate_id = {});

    [[nodiscard]] ApiResponse handle(const ApiRequest& request);
    [[nodiscard]] std::size_t active_session_count() const noexcept;
    void expire_idle_sessions();

private:
    struct SessionRecord {
        std::unique_ptr<session::Session> session;
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point last_used_at;
    };

    EngineApiLimits limits_;
    Clock clock_;
    IdGenerator generate_id_;
    std::unordered_map<std::string, SessionRecord> sessions_;
};

}  // namespace aleph3::web
