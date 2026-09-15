#include "aleph_client/RuntimeClient.hpp"

#include "aleph_client/Framing.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <sstream>
#include <thread>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace aleph3::client {
namespace {
using Clock = std::chrono::steady_clock;

#ifdef _WIN32
constexpr const char* default_runtime_name = "aleph-runtime.exe";
#else
constexpr const char* default_runtime_name = "aleph-runtime";
#endif

std::string quote_path(const std::filesystem::path& path) {
    return path.string();
}

std::string lookup_env(const RuntimeClientOptions& options, const std::string& name) {
    if (const auto found = options.environment.find(name); found != options.environment.end()) {
        return found->second;
    }
    const char* value = std::getenv(name.c_str());
    return value == nullptr ? std::string{} : std::string(value);
}

std::vector<std::filesystem::path> split_path_list(const std::string& paths) {
    std::vector<std::filesystem::path> result;
#ifdef _WIN32
    constexpr char separator = ';';
#else
    constexpr char separator = ':';
#endif
    std::size_t start = 0;
    while (start <= paths.size()) {
        const auto end = paths.find(separator, start);
        const auto item = paths.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!item.empty()) {
            result.emplace_back(item);
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return result;
}

std::optional<std::filesystem::path> current_executable_directory() {
#ifdef _WIN32
    std::array<char, MAX_PATH> buffer{};
    const auto length = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length == buffer.size()) return std::nullopt;
    return std::filesystem::path(std::string(buffer.data(), length)).parent_path();
#else
    std::array<char, 4096> buffer{};
    const auto length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length <= 0) return std::nullopt;
    return std::filesystem::path(std::string(buffer.data(), static_cast<std::size_t>(length))).parent_path();
#endif
}

bool is_regular_candidate(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::exists(path, error) && std::filesystem::is_regular_file(path, error);
}

#ifndef _WIN32
bool is_executable_candidate(const std::filesystem::path& path) {
    return is_regular_candidate(path) && access(path.string().c_str(), X_OK) == 0;
}
#else
bool is_executable_candidate(const std::filesystem::path& path) {
    return is_regular_candidate(path);
}
#endif

std::string error_message_with_path(std::string_view message, const std::filesystem::path& path) {
    return std::string(message) + ": " + path.string();
}

std::string read_until(
    auto&& read_one,
    std::string_view terminator,
    std::chrono::milliseconds timeout,
    std::string_view timeout_message) {
    const auto deadline = Clock::now() + timeout;
    std::string data;
    while (data.find(terminator) == std::string::npos) {
        if (Clock::now() >= deadline) {
            throw RuntimeClientException("runtime.timeout", std::string(timeout_message));
        }
        char ch = '\0';
        const auto state = read_one(&ch, 1, deadline);
        if (state == 0) {
            throw RuntimeClientException("runtime.exited", "Runtime exited before a complete response was read.");
        }
        data.push_back(ch);
        if (data.size() > 1024) {
            throw RuntimeClientException("runtime.malformed_output", "Runtime frame header is too large.");
        }
    }
    return data;
}

std::size_t parse_content_length(std::string_view header) {
    constexpr std::string_view prefix = "Content-Length: ";
    if (!header.starts_with(prefix)) {
        throw RuntimeClientException("runtime.malformed_output", "Runtime output did not start with Content-Length.");
    }
    const auto value = header.substr(prefix.size());
    std::size_t length = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto parsed = std::from_chars(begin, end, length);
    if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != end) {
        throw RuntimeClientException("runtime.malformed_output", "Runtime output contained an invalid Content-Length.");
    }
    return length;
}

}  // namespace

RuntimeClientException::RuntimeClientException(std::string code, std::string message)
    : std::runtime_error(message), code_(std::move(code)) {}

RuntimeClientException::RuntimeClientException(std::string code, std::string message, ProtocolError protocol_error)
    : std::runtime_error(message), code_(std::move(code)), protocol_error_(std::move(protocol_error)) {}

const std::string& RuntimeClientException::code() const noexcept {
    return code_;
}

const std::optional<ProtocolError>& RuntimeClientException::protocol_error() const noexcept {
    return protocol_error_;
}

const char* to_string(RuntimeLookupSource source) noexcept {
    switch (source) {
        case RuntimeLookupSource::explicit_path: return "explicit";
        case RuntimeLookupSource::environment: return "environment";
        case RuntimeLookupSource::same_directory: return "same_directory";
        case RuntimeLookupSource::path: return "path";
    }
    return "unknown";
}

RuntimeLookupResult resolve_runtime(const RuntimeClientOptions& options) {
    auto name = options.runtime_name.empty() ? std::string(default_runtime_name) : options.runtime_name;
    if (options.runtime_path.has_value()) {
        if (!is_executable_candidate(*options.runtime_path)) {
            throw RuntimeClientException("runtime.not_found", error_message_with_path("Runtime executable was not found", *options.runtime_path));
        }
        return {*options.runtime_path, RuntimeLookupSource::explicit_path};
    }

    if (auto env_path = lookup_env(options, "ALEPH_RUNTIME_PATH"); !env_path.empty()) {
        std::filesystem::path path(env_path);
        if (!is_executable_candidate(path)) {
            throw RuntimeClientException("runtime.not_found", error_message_with_path("ALEPH_RUNTIME_PATH did not name a launchable file", path));
        }
        return {path, RuntimeLookupSource::environment};
    }

    if (const auto directory = current_executable_directory()) {
        const auto candidate = *directory / name;
        if (is_executable_candidate(candidate)) {
            return {candidate, RuntimeLookupSource::same_directory};
        }
    }

    for (const auto& directory : split_path_list(lookup_env(options, "PATH"))) {
        const auto candidate = directory / name;
        if (is_executable_candidate(candidate)) {
            return {candidate, RuntimeLookupSource::path};
        }
    }

    throw RuntimeClientException("runtime.not_found", "No aleph-runtime executable could be resolved.");
}

struct RuntimeClient::Impl {
#ifdef _WIN32
    HANDLE process = nullptr;
    HANDLE thread = nullptr;
    HANDLE child_stdin = nullptr;
    HANDLE child_stdout = nullptr;
#else
    pid_t pid = -1;
    int child_stdin = -1;
    int child_stdout = -1;
#endif

    bool started = false;

    ~Impl() {
        cleanup_process();
    }

    void cleanup_process() noexcept {
#ifdef _WIN32
        if (process != nullptr) {
            DWORD exit_code = 0;
            if (GetExitCodeProcess(process, &exit_code) && exit_code == STILL_ACTIVE) {
                TerminateProcess(process, 3);
                WaitForSingleObject(process, 500);
            }
        }
        if (child_stdin != nullptr) CloseHandle(child_stdin);
        if (child_stdout != nullptr) CloseHandle(child_stdout);
        if (thread != nullptr) CloseHandle(thread);
        if (process != nullptr) CloseHandle(process);
        child_stdin = nullptr;
        child_stdout = nullptr;
        thread = nullptr;
        process = nullptr;
#else
        if (child_stdin >= 0) close(child_stdin);
        if (child_stdout >= 0) close(child_stdout);
        child_stdin = -1;
        child_stdout = -1;
        if (pid > 0) {
            int status = 0;
            if (waitpid(pid, &status, WNOHANG) == 0) {
                kill(pid, SIGTERM);
                waitpid(pid, &status, 0);
            }
        }
        pid = -1;
#endif
        started = false;
    }

    void launch(const RuntimeLookupResult& lookup, const RuntimeClientOptions& options) {
#ifdef _WIN32
        SECURITY_ATTRIBUTES security{};
        security.nLength = sizeof(SECURITY_ATTRIBUTES);
        security.bInheritHandle = TRUE;

        HANDLE stdout_read = nullptr;
        HANDLE stdout_write = nullptr;
        HANDLE stdin_read = nullptr;
        HANDLE stdin_write = nullptr;
        if (!CreatePipe(&stdout_read, &stdout_write, &security, 0) ||
            !SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0) ||
            !CreatePipe(&stdin_read, &stdin_write, &security, 0) ||
            !SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0)) {
            if (stdout_read) CloseHandle(stdout_read);
            if (stdout_write) CloseHandle(stdout_write);
            if (stdin_read) CloseHandle(stdin_read);
            if (stdin_write) CloseHandle(stdin_write);
            throw RuntimeClientException("runtime.launch_failed", "Failed to create runtime pipes.");
        }

        std::string command = "\"" + quote_path(lookup.executable_path) + "\"";
        for (const auto& arg : options.runtime_arguments) {
            command += " \"" + arg + "\"";
        }

        STARTUPINFOA startup{};
        startup.cb = sizeof(STARTUPINFOA);
        startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        startup.hStdOutput = stdout_write;
        startup.hStdInput = stdin_read;
        startup.dwFlags |= STARTF_USESTDHANDLES;

        PROCESS_INFORMATION process_info{};
        std::vector<char> command_line(command.begin(), command.end());
        command_line.push_back('\0');
        const BOOL ok = CreateProcessA(
            nullptr,
            command_line.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startup,
            &process_info);
        CloseHandle(stdout_write);
        CloseHandle(stdin_read);
        if (!ok) {
            CloseHandle(stdout_read);
            CloseHandle(stdin_write);
            throw RuntimeClientException("runtime.not_executable", error_message_with_path("Runtime could not be launched", lookup.executable_path));
        }
        process = process_info.hProcess;
        thread = process_info.hThread;
        child_stdout = stdout_read;
        child_stdin = stdin_write;
#else
        int stdin_pipe[2]{};
        int stdout_pipe[2]{};
        if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0) {
            throw RuntimeClientException("runtime.launch_failed", "Failed to create runtime pipes.");
        }
        pid = fork();
        if (pid < 0) {
            close(stdin_pipe[0]);
            close(stdin_pipe[1]);
            close(stdout_pipe[0]);
            close(stdout_pipe[1]);
            throw RuntimeClientException("runtime.launch_failed", "Failed to fork runtime process.");
        }
        if (pid == 0) {
            dup2(stdin_pipe[0], STDIN_FILENO);
            dup2(stdout_pipe[1], STDOUT_FILENO);
            close(stdin_pipe[0]);
            close(stdin_pipe[1]);
            close(stdout_pipe[0]);
            close(stdout_pipe[1]);
            std::vector<std::string> args_storage;
            args_storage.push_back(lookup.executable_path.string());
            args_storage.insert(args_storage.end(), options.runtime_arguments.begin(), options.runtime_arguments.end());
            std::vector<char*> argv;
            for (auto& arg : args_storage) argv.push_back(arg.data());
            argv.push_back(nullptr);
            execv(lookup.executable_path.string().c_str(), argv.data());
            _exit(127);
        }
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        child_stdin = stdin_pipe[1];
        child_stdout = stdout_pipe[0];
        fcntl(child_stdout, F_SETFL, fcntl(child_stdout, F_GETFL, 0) | O_NONBLOCK);
#endif
        started = true;
    }

    void write_all(const std::string& data) {
        std::size_t written = 0;
        while (written < data.size()) {
#ifdef _WIN32
            DWORD count = 0;
            if (!WriteFile(child_stdin, data.data() + written, static_cast<DWORD>(data.size() - written), &count, nullptr) || count == 0) {
                throw RuntimeClientException("runtime.broken_pipe", "Failed to write request to runtime.");
            }
#else
            const auto count = write(child_stdin, data.data() + written, data.size() - written);
            if (count <= 0) {
                throw RuntimeClientException("runtime.broken_pipe", "Failed to write request to runtime.");
            }
#endif
            written += static_cast<std::size_t>(count);
        }
    }

    int read_some(char* buffer, std::size_t size, Clock::time_point deadline) {
#ifdef _WIN32
        while (Clock::now() < deadline) {
            DWORD exit_code = 0;
            if (GetExitCodeProcess(process, &exit_code) && exit_code != STILL_ACTIVE) {
                DWORD available = 0;
                if (!PeekNamedPipe(child_stdout, nullptr, 0, nullptr, &available, nullptr) || available == 0) return 0;
            }
            DWORD available = 0;
            if (!PeekNamedPipe(child_stdout, nullptr, 0, nullptr, &available, nullptr)) {
                return 0;
            }
            if (available > 0) {
                DWORD read = 0;
                if (!ReadFile(child_stdout, buffer, static_cast<DWORD>(std::min<std::size_t>(size, available)), &read, nullptr)) {
                    return 0;
                }
                return static_cast<int>(read);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        throw RuntimeClientException("runtime.timeout", "Timed out waiting for runtime response.");
#else
        while (Clock::now() < deadline) {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());
            pollfd fd{child_stdout, POLLIN, 0};
            const int ready = poll(&fd, 1, static_cast<int>(std::max<std::int64_t>(1, remaining.count())));
            if (ready > 0 && (fd.revents & POLLIN) != 0) {
                const auto count = read(child_stdout, buffer, size);
                if (count <= 0) return 0;
                return static_cast<int>(count);
            }
            if (ready > 0 && (fd.revents & (POLLHUP | POLLERR | POLLNVAL)) != 0) return 0;
            if (ready < 0 && errno != EINTR) return 0;
        }
        throw RuntimeClientException("runtime.timeout", "Timed out waiting for runtime response.");
#endif
    }

    std::string read_frame(std::chrono::milliseconds timeout, std::size_t max_frame_bytes) {
        auto read_one = [this](char* buffer, std::size_t size, Clock::time_point deadline) {
            return read_some(buffer, size, deadline);
        };
        auto header = read_until(read_one, "\r\n\r\n", timeout, "Timed out waiting for runtime frame header.");
        header.resize(header.size() - 4);
        const auto length = parse_content_length(header);
        if (length > max_frame_bytes) {
            throw RuntimeClientException("runtime.malformed_output", "Runtime response exceeded the configured frame size.");
        }
        const auto deadline = Clock::now() + timeout;
        std::string payload(length, '\0');
        std::size_t offset = 0;
        while (offset < length) {
            const auto count = read_some(payload.data() + offset, length - offset, deadline);
            if (count == 0) {
                throw RuntimeClientException("runtime.exited", "Runtime exited before a complete response was read.");
            }
            offset += static_cast<std::size_t>(count);
        }
        return payload;
    }

    bool wait_for_exit(std::chrono::milliseconds timeout) {
#ifdef _WIN32
        if (process == nullptr) return true;
        return WaitForSingleObject(process, static_cast<DWORD>(timeout.count())) == WAIT_OBJECT_0;
#else
        if (pid <= 0) return true;
        const auto deadline = Clock::now() + timeout;
        int status = 0;
        while (Clock::now() < deadline) {
            const auto result = waitpid(pid, &status, WNOHANG);
            if (result == pid) {
                pid = -1;
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
#endif
    }
};

RuntimeClient::RuntimeClient(RuntimeClientOptions options)
    : options_(std::move(options)), impl_(std::make_unique<Impl>()) {
    if (options_.runtime_name.empty()) {
        options_.runtime_name = default_runtime_name;
    }
}

RuntimeClient::~RuntimeClient() = default;
RuntimeClient::RuntimeClient(RuntimeClient&&) noexcept = default;
RuntimeClient& RuntimeClient::operator=(RuntimeClient&&) noexcept = default;

const RuntimeLookupResult& RuntimeClient::lookup_result() const {
    return lookup_;
}

bool RuntimeClient::running() const noexcept {
    return impl_ != nullptr && impl_->started;
}

RequestId RuntimeClient::next_id() {
    return RequestId::number(next_request_id_++);
}

InitializeResult RuntimeClient::start() {
    if (running()) {
        throw RuntimeClientException("runtime.launch_failed", "Runtime client is already started.");
    }
    lookup_ = resolve_runtime(options_);
    impl_->launch(lookup_, options_);
    const auto response = send_payload(
        encode_initialize_request(next_id(), {options_.client_name, options_.client_version, options_.protocol_version}),
        options_.startup_timeout);
    if (response.error.has_value()) {
        const auto code = response.error->code == "protocol.incompatible_version" ? "runtime.incompatible_version" : "runtime.initialize_failed";
        throw RuntimeClientException(code, response.error->message, *response.error);
    }
    if (!response.initialize.has_value()) {
        throw RuntimeClientException("runtime.initialize_failed", "Runtime initialize did not return initialize metadata.");
    }
    if (response.initialize->protocol_version != options_.protocol_version) {
        throw RuntimeClientException("runtime.incompatible_version", "Runtime returned an incompatible protocol version.");
    }
    return *response.initialize;
}

ProtocolResponse RuntimeClient::send_payload(std::string payload, std::chrono::milliseconds timeout) {
    if (!running()) {
        throw RuntimeClientException("runtime.exited", "Runtime process is not running.");
    }
    impl_->write_all(write_frame(payload));
    const auto frame = impl_->read_frame(timeout, options_.max_frame_bytes);
    try {
        return decode_response(frame);
    } catch (const ProtocolException& error) {
        throw RuntimeClientException("runtime.malformed_output", error.what());
    }
}

ProtocolResponse RuntimeClient::request(
    std::string_view method,
    const std::map<std::string, std::string>& string_params,
    std::chrono::milliseconds timeout) {
    if (timeout == std::chrono::milliseconds{0}) {
        timeout = options_.request_timeout;
    }
    return send_payload(encode_request(next_id(), method, string_params), timeout);
}

ProtocolResponse RuntimeClient::evaluate(std::string_view source) {
    return request("evaluate", {{"source", std::string(source)}});
}

ProtocolResponse RuntimeClient::simplify(std::string_view source) {
    return request("simplify", {{"source", std::string(source)}});
}

ProtocolResponse RuntimeClient::full_form(std::string_view source) {
    return request("fullForm", {{"source", std::string(source)}});
}

ProtocolResponse RuntimeClient::help(std::string_view query) {
    return request("help", {{"query", std::string(query)}});
}

ProtocolResponse RuntimeClient::complete(std::string_view query) {
    return request("complete", {{"query", std::string(query)}});
}

ProtocolResponse RuntimeClient::packages() {
    return request("packages");
}

ProtocolResponse RuntimeClient::capabilities() {
    return request("capabilities");
}

ProtocolResponse RuntimeClient::version() {
    return request("version");
}

ProtocolResponse RuntimeClient::reset() {
    return request("reset");
}

void RuntimeClient::shutdown() {
    if (!running()) return;
    try {
        const auto response = request("shutdown", {}, options_.shutdown_timeout);
        if (response.error.has_value()) {
            throw RuntimeClientException("runtime.shutdown_failed", response.error->message, *response.error);
        }
    } catch (const RuntimeClientException& error) {
        if (std::string(error.code()) != "runtime.exited") {
            impl_->cleanup_process();
            throw;
        }
    }
    if (!impl_->wait_for_exit(options_.shutdown_timeout)) {
        impl_->cleanup_process();
        throw RuntimeClientException("runtime.shutdown_failed", "Runtime did not exit after shutdown.");
    }
    impl_->cleanup_process();
}

}  // namespace aleph3::client
