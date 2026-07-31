#include "web/EngineApi.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

namespace {

volatile std::sig_atomic_t g_running = 1;

void handle_signal(int) {
    g_running = 0;
}

void close_socket(SocketHandle socket) {
#if defined(_WIN32)
    closesocket(socket);
#else
    close(socket);
#endif
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::optional<std::size_t> content_length(const std::map<std::string, std::string>& headers) {
    const auto it = headers.find("content-length");
    if (it == headers.end()) {
        return std::nullopt;
    }
    std::size_t value = 0;
    const auto text = it->second;
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        return std::nullopt;
    }
    return value;
}

std::string reason_phrase(int status) {
    switch (status) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        default: return "Error";
    }
}

std::string http_response(const aleph3::web::ApiResponse& response) {
    std::ostringstream out;
    out << "HTTP/1.1 " << response.status << ' ' << reason_phrase(response.status) << "\r\n";
    for (const auto& [name, value] : response.headers) {
        out << name << ": " << value << "\r\n";
    }
    out << "Content-Length: " << response.body.size() << "\r\n";
    out << "Connection: close\r\n\r\n";
    out << response.body;
    return out.str();
}

std::string receive_request_bytes(SocketHandle client) {
    std::string data;
    char buffer[4096];
    std::size_t expected_body = 0;
    bool headers_parsed = false;

    while (data.size() < 2u * 1024u * 1024u) {
        const int received = recv(client, buffer, sizeof(buffer), 0);
        if (received <= 0) {
            break;
        }
        data.append(buffer, static_cast<std::size_t>(received));
        const auto header_end = data.find("\r\n\r\n");
        if (!headers_parsed && header_end != std::string::npos) {
            headers_parsed = true;
            std::istringstream header_stream(data.substr(0, header_end));
            std::string request_line;
            std::getline(header_stream, request_line);
            std::string line;
            std::map<std::string, std::string> headers;
            while (std::getline(header_stream, line)) {
                const auto colon = line.find(':');
                if (colon != std::string::npos) {
                    headers[lower_ascii(trim(line.substr(0, colon)))] = trim(line.substr(colon + 1));
                }
            }
            expected_body = content_length(headers).value_or(0);
        }
        if (headers_parsed) {
            const auto header_end_after = data.find("\r\n\r\n");
            if (header_end_after != std::string::npos &&
                data.size() >= header_end_after + 4 + expected_body) {
                break;
            }
        }
    }
    return data;
}

aleph3::web::ApiRequest parse_http_request(const std::string& data) {
    const auto header_end = data.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        throw std::invalid_argument("HTTP headers are incomplete.");
    }

    std::istringstream stream(data.substr(0, header_end));
    std::string method;
    std::string target;
    std::string version;
    stream >> method >> target >> version;
    if (method.empty() || target.empty()) {
        throw std::invalid_argument("HTTP request line is invalid.");
    }

    std::string line;
    std::getline(stream, line);
    std::map<std::string, std::string> headers;
    while (std::getline(stream, line)) {
        const auto colon = line.find(':');
        if (colon != std::string::npos) {
            headers[trim(line.substr(0, colon))] = trim(line.substr(colon + 1));
        }
    }

    return {method, target, std::move(headers), data.substr(header_end + 4)};
}

SocketHandle bind_listener(std::uint16_t port) {
    SocketHandle listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == kInvalidSocket) {
        throw std::runtime_error("socket creation failed");
    }

    int reuse = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close_socket(listener);
        throw std::runtime_error("socket bind failed");
    }
    if (listen(listener, 16) != 0) {
        close_socket(listener);
        throw std::runtime_error("socket listen failed");
    }
    return listener;
}

std::uint16_t port_from_environment() {
    const char* port_text = std::getenv("ALEPH3_ENGINE_PORT");
    if (port_text == nullptr || std::strlen(port_text) == 0) {
        return 8080;
    }
    unsigned long value = std::strtoul(port_text, nullptr, 10);
    if (value == 0 || value > 65535) {
        throw std::runtime_error("ALEPH3_ENGINE_PORT must be from 1 through 65535");
    }
    return static_cast<std::uint16_t>(value);
}

}  // namespace

int main(int argc, char** argv) {
    aleph3::web::EngineApi api;

    if (argc == 2 && std::string(argv[1]) == "--health") {
        const auto response = api.handle({"GET", "/internal/health", {}, ""});
        std::cout << response.body << '\n';
        return response.status == 200 ? 0 : 1;
    }

#if defined(_WIN32)
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        std::cerr << "Winsock startup failed.\n";
        return 1;
    }
#endif

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    try {
        const auto port = port_from_environment();
        const auto listener = bind_listener(port);
        std::cout << "aleph3_engine_service listening on port " << port << '\n';
        while (g_running) {
            const auto client = accept(listener, nullptr, nullptr);
            if (client == kInvalidSocket) {
                continue;
            }
            try {
                const auto request = parse_http_request(receive_request_bytes(client));
                const auto response = http_response(api.handle(request));
                send(client, response.data(), static_cast<int>(response.size()), 0);
            } catch (const std::exception& error) {
                const auto response = http_response({
                    400,
                    {{"Content-Type", "application/json; charset=utf-8"}},
                    std::string(R"({"status":"error","error":{"code":"engine.invalid_http","message":")") +
                        error.what() + R"("}})"});
                send(client, response.data(), static_cast<int>(response.size()), 0);
            }
            close_socket(client);
        }
        close_socket(listener);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
#if defined(_WIN32)
        WSACleanup();
#endif
        return 1;
    }

#if defined(_WIN32)
    WSACleanup();
#endif
    return 0;
}
