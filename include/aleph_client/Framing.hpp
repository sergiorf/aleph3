#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace aleph3::client {

class FramingException : public std::runtime_error {
public:
    FramingException(std::string code, std::string message);

    [[nodiscard]] const std::string& code() const noexcept;

private:
    std::string code_;
};

[[nodiscard]] std::string write_frame(std::string_view payload);
[[nodiscard]] std::vector<std::string> read_frames(
    std::string_view bytes,
    std::size_t max_payload_bytes = 8u * 1024u * 1024u);

}  // namespace aleph3::client
