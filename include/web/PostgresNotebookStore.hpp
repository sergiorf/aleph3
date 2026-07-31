/* Postgres-backed notebook persistence for cloud-hosted Aleph3 web deployments. */
#pragma once

#include "web/NotebookStore.hpp"

#include <memory>
#include <string>

namespace aleph3::web {

[[nodiscard]] std::unique_ptr<NotebookStore> make_postgres_notebook_store(
    const std::string& connection_string);

}  // namespace aleph3::web
