#include "aleph_client/Framing.hpp"
#include "tooling/RuntimeProtocolServer.hpp"

#include <charconv>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {
constexpr std::string_view content_length_header = "Content-Length: ";
constexpr std::size_t max_payload_bytes = 8u * 1024u * 1024u;

std::optional<std::string> read_frame(std::istream& input) {
    std::string header;
    if (!std::getline(input, header)) {
        return std::nullopt;
    }
    if (!header.ends_with('\r')) {
        throw aleph3::client::FramingException("protocol.missing_header_separator", "Frame header is missing the CRLF separator.");
    }
    header.pop_back();
    if (!header.starts_with(content_length_header)) {
        throw aleph3::client::FramingException("protocol.invalid_header", "Frame must start with Content-Length.");
    }

    std::size_t length = 0;
    const auto value = std::string_view(header).substr(content_length_header.size());
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto parsed = std::from_chars(begin, end, length);
    if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != end) {
        throw aleph3::client::FramingException("protocol.invalid_content_length", "Content-Length must be a non-negative integer.");
    }
    if (length > max_payload_bytes) {
        throw aleph3::client::FramingException("protocol.frame_too_large", "Frame payload exceeds the configured limit.");
    }

    std::string separator;
    if (!std::getline(input, separator) || separator != "\r") {
        throw aleph3::client::FramingException("protocol.missing_header_separator", "Frame header is missing the CRLF separator.");
    }

    std::string payload(length, '\0');
    input.read(payload.data(), static_cast<std::streamsize>(payload.size()));
    if (input.gcount() != static_cast<std::streamsize>(payload.size())) {
        throw aleph3::client::FramingException("protocol.truncated_frame", "Frame body is shorter than Content-Length.");
    }
    return payload;
}
}  // namespace

int main() {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    std::ios::sync_with_stdio(false);

    aleph3::tooling::RuntimeProtocolServer server;
    try {
        while (auto frame = read_frame(std::cin)) {
            std::cout << aleph3::client::write_frame(server.handle_payload(*frame));
            std::cout.flush();
            if (server.shutdown_requested()) {
                return 0;
            }
        }
    } catch (const aleph3::client::FramingException& error) {
        std::cout << aleph3::client::write_frame(
            aleph3::tooling::protocol_error_response(error.code(), error.what()));
        std::cout.flush();
        return 2;
    } catch (const std::exception& error) {
        std::cout << aleph3::client::write_frame(
            aleph3::tooling::protocol_error_response("runtime.io_error", error.what()));
        std::cout.flush();
        return 2;
    }
    return 0;
}
