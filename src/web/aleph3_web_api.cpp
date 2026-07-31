#include "web/WebApi.hpp"
#include "web/NotebookStore.hpp"

#if defined(ALEPH3_ENABLE_POSTGRES)
#include "web/PostgresNotebookStore.hpp"
#endif

#include <cstdlib>
#include <iostream>
#include <memory>

int main(int argc, char** argv) {
    std::unique_ptr<aleph3::web::WebApi> api;
#if defined(ALEPH3_ENABLE_POSTGRES)
    if (const char* database_url = std::getenv("ALEPH3_DATABASE_URL")) {
        api = std::make_unique<aleph3::web::WebApi>(
            aleph3::web::ApiLimits{},
            aleph3::web::WebApi::Clock{},
            aleph3::web::WebApi::IdGenerator{},
            aleph3::web::make_postgres_notebook_store(database_url));
    }
#endif
    if (!api) {
        api = std::make_unique<aleph3::web::WebApi>();
    }
    if (argc == 2 && std::string(argv[1]) == "--health") {
        const auto response = api->handle({"GET", "/api/health", {}, ""});
        std::cout << response.body << '\n';
        return response.status == 200 ? 0 : 1;
    }

    std::cerr
        << "aleph3_web_api currently exposes the transport-independent API core.\n"
        << "Run `aleph3_web_api --health` for a local smoke check.\n";
    return 0;
}
