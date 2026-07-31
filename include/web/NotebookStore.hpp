/* Notebook persistence boundary for the Aleph3 web API. */
#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace aleph3::web {

struct StoredNotebook {
    std::string id;
    std::string anonymous_client_id;
    std::string title;
    std::string document_json;
    std::string created_at;
    std::string updated_at;
    std::string last_opened_at;
    std::size_t size_bytes = 0;
};

class NotebookStoreError : public std::runtime_error {
public:
    NotebookStoreError(std::string code, std::string message);
    [[nodiscard]] const std::string& code() const noexcept;

private:
    std::string code_;
};

class NotebookStore {
public:
    virtual ~NotebookStore() = default;

    virtual void create_client(const std::string& client_id, const std::string& created_at) = 0;
    [[nodiscard]] virtual bool client_exists(const std::string& client_id) const = 0;

    [[nodiscard]] virtual std::size_t notebook_count_for_client(const std::string& client_id) const = 0;
    [[nodiscard]] virtual std::size_t stored_bytes_for_client(const std::string& client_id) const = 0;

    virtual void create_notebook(const StoredNotebook& notebook) = 0;
    [[nodiscard]] virtual std::vector<StoredNotebook> list_notebooks(const std::string& client_id) const = 0;
    [[nodiscard]] virtual std::optional<StoredNotebook> get_notebook(const std::string& notebook_id) const = 0;
    virtual void update_notebook(
        const std::string& notebook_id,
        const std::string& title,
        const std::string& document_json,
        const std::string& updated_at,
        std::size_t size_bytes) = 0;
    virtual void touch_notebook(const std::string& notebook_id, const std::string& opened_at) = 0;
    virtual void delete_notebook(const std::string& notebook_id) = 0;
};

[[nodiscard]] std::unique_ptr<NotebookStore> make_memory_notebook_store();

}  // namespace aleph3::web
