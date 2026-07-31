#include "web/WebApi.hpp"

#include <iostream>

int main(int argc, char** argv) {
    aleph3::web::WebApi api;
    if (argc == 2 && std::string(argv[1]) == "--health") {
        const auto response = api.handle({"GET", "/api/health", {}, ""});
        std::cout << response.body << '\n';
        return response.status == 200 ? 0 : 1;
    }

    std::cerr
        << "aleph3_web_api currently exposes the transport-independent API core.\n"
        << "Run `aleph3_web_api --health` for a local smoke check.\n";
    return 0;
}
