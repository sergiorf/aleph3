#include "aleph_client/Framing.hpp"

#include "json.hpp"

#include <chrono>
#include <charconv>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {
using Json = nlohmann::json;

std::string mode_from_args(int argc, char** argv) {
    return argc > 1 ? std::string(argv[1]) : std::string("normal");
}

std::optional<std::string> read_frame(std::istream& input) {
    std::string header;
    if (!std::getline(input, header)) return std::nullopt;
    if (!header.ends_with('\r')) return std::nullopt;
    header.pop_back();
    constexpr std::string_view prefix = "Content-Length: ";
    if (!std::string_view(header).starts_with(prefix)) return std::nullopt;
    const auto value = std::string_view(header).substr(prefix.size());
    std::size_t length = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), length);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) return std::nullopt;
    std::string separator;
    if (!std::getline(input, separator) || separator != "\r") return std::nullopt;
    std::string payload(length, '\0');
    input.read(payload.data(), static_cast<std::streamsize>(payload.size()));
    if (input.gcount() != static_cast<std::streamsize>(payload.size())) return std::nullopt;
    return payload;
}

Json result_response(const Json& request, Json result) {
    return {{"jsonrpc", "2.0"}, {"id", request.value("id", Json(nullptr))}, {"result", std::move(result)}};
}

Json error_response(const Json& request, std::string code, std::string message) {
    return {
        {"jsonrpc", "2.0"},
        {"id", request.value("id", Json(nullptr))},
        {"error", {{"code", std::move(code)}, {"message", std::move(message)}, {"diagnostics", Json::array()}}}};
}

Json evaluation_result(std::string source, std::string text, bool ok = true) {
    return {
        {"ok", ok},
        {"source", source},
        {"representations", {{"text/plain", text}, {"text/x-aleph-source", text}}},
        {"diagnostics", Json::array()}};
}

std::string param_string(const Json& request, const char* name) {
    if (!request.contains("params") || !request["params"].contains(name) || !request["params"][name].is_string()) {
        return "";
    }
    return request["params"][name].get<std::string>();
}

Json handle_request(const Json& request, const std::string& mode, bool& assigned, bool& shutdown) {
    const auto method = request.value("method", "");
    if (mode == "incompatible" && method == "initialize") {
        return error_response(request, "protocol.incompatible_version", "Unsupported protocol version.");
    }
    if (mode == "protocol-error" && method == "evaluate") {
        return error_response(request, "protocol.invalid_params", "fake protocol error");
    }
    if (method == "initialize") {
        return result_response(
            request,
            {{"serverName", "fake-runtime"},
             {"kernelVersion", "0"},
             {"protocolVersion", 1},
             {"capabilities", {{"evaluate", true}, {"reset", true}}}});
    }
    if (method == "version") {
        return result_response(request, {{"serverName", "fake-runtime"}, {"kernelVersion", "0"}, {"protocolVersion", 1}});
    }
    if (method == "capabilities") {
        return result_response(
            request,
            {{"methods", {"initialize", "version", "capabilities", "evaluate", "simplify", "fullForm", "help", "complete", "packages", "reset", "shutdown"}},
             {"representations", {"text/plain", "text/x-aleph-source"}},
             {"features", {{"statefulSession", true}}}});
    }
    if (method == "evaluate" || method == "simplify" || method == "fullForm") {
        const auto source = param_string(request, "source");
        if (source == "a = 7") {
            assigned = true;
            return result_response(request, evaluation_result(source, "7"));
        }
        if (source == "a") {
            return result_response(request, evaluation_result(source, assigned ? "7" : "a"));
        }
        return result_response(request, evaluation_result(source, source == "1/2 + 1/3" ? "5/6" : source));
    }
    if (method == "help") {
        return result_response(request, {{"help", {{{"name", param_string(request, "query")}, {"description", "fake help"}}}}});
    }
    if (method == "complete") {
        return result_response(request, {{"completions", {{{"name", "Factor"}, {"category", "function"}}}}});
    }
    if (method == "packages") {
        return result_response(request, {{"packages", {{{"name", "core-algebra"}, {"version", "0"}, {"symbols", {"Factor"}}}}}});
    }
    if (method == "reset") {
        assigned = false;
        return result_response(request, {{"ok", true}, {"message", "reset"}});
    }
    if (method == "shutdown") {
        shutdown = true;
        return result_response(request, {{"ok", true}, {"message", "shutdown"}});
    }
    return error_response(request, "protocol.unknown_method", "unknown method");
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    std::ios::sync_with_stdio(false);
    const auto mode = mode_from_args(argc, argv);
    if (mode == "malformed") {
        std::cout << "Other: 2\r\n\r\n{}";
        std::cout.flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        return 0;
    }
    if (mode == "early-exit") {
        return 0;
    }
    bool assigned = false;
    bool shutdown = false;
    while (auto payload = read_frame(std::cin)) {
        if (mode == "delay") {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
        const auto request = Json::parse(*payload);
        std::cout << aleph3::client::write_frame(handle_request(request, mode, assigned, shutdown).dump());
        std::cout.flush();
        if (shutdown) return 0;
    }
    return 0;
}
