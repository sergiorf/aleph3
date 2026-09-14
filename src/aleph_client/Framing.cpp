#include "aleph_client/Framing.hpp"

#include <charconv>
#include <limits>
#include <string>
#include <utility>

namespace aleph3::client {
namespace {
constexpr std::string_view header_name = "Content-Length: ";
constexpr std::string_view separator = "\r\n\r\n";

std::size_t parse_length(std::string_view value) {
    if (value.empty()) {
        throw FramingException("protocol.invalid_content_length", "Content-Length must not be empty.");
    }
    std::size_t length = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, length);
    if (result.ec != std::errc{} || result.ptr != end) {
        throw FramingException("protocol.invalid_content_length", "Content-Length must be a non-negative integer.");
    }
    return length;
}
}  // namespace

FramingException::FramingException(std::string code, std::string message)
    : std::runtime_error(message), code_(std::move(code)) {}

const std::string& FramingException::code() const noexcept {
    return code_;
}

std::string write_frame(std::string_view payload) {
    std::string frame;
    frame.reserve(header_name.size() + 20 + separator.size() + payload.size());
    frame.append(header_name);
    frame.append(std::to_string(payload.size()));
    frame.append(separator);
    frame.append(payload);
    return frame;
}

std::vector<std::string> read_frames(std::string_view bytes, std::size_t max_payload_bytes) {
    std::vector<std::string> frames;
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto header_end = bytes.find(separator, offset);
        if (header_end == std::string_view::npos) {
            throw FramingException("protocol.missing_header_separator", "Frame header is missing the CRLF separator.");
        }

        const auto header = bytes.substr(offset, header_end - offset);
        if (!header.starts_with(header_name)) {
            throw FramingException("protocol.invalid_header", "Frame must start with Content-Length.");
        }
        const auto length = parse_length(header.substr(header_name.size()));
        if (length > max_payload_bytes) {
            throw FramingException("protocol.frame_too_large", "Frame payload exceeds the configured limit.");
        }

        const auto body_start = header_end + separator.size();
        if (length > bytes.size() - body_start) {
            throw FramingException("protocol.truncated_frame", "Frame body is shorter than Content-Length.");
        }

        frames.emplace_back(bytes.substr(body_start, length));
        offset = body_start + length;
    }
    return frames;
}

}  // namespace aleph3::client
