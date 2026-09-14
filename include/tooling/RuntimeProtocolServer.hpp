#pragma once

#include "session/Session.hpp"

#include <string>
#include <string_view>

namespace aleph3::tooling {

class RuntimeProtocolServer {
public:
    [[nodiscard]] std::string handle_payload(std::string_view payload);
    [[nodiscard]] bool shutdown_requested() const noexcept;

private:
    session::Session session_;
    bool shutdown_requested_ = false;
};

[[nodiscard]] std::string protocol_error_response(
    std::string_view code,
    std::string_view message);

}  // namespace aleph3::tooling
