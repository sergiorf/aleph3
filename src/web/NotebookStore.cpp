#include "web/NotebookStore.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace aleph3::web {

NotebookStoreError::NotebookStoreError(std::string code, std::string message)
    : std::runtime_error(std::move(message)), code_(std::move(code)) {
}

const std::string& NotebookStoreError::code() const noexcept {
    return code_;
}

namespace {

class MemoryNotebookStore final : public NotebookStore {
public:
    void create_client(const std::string& client_id, const std::string&) override {
        clients_.insert(client_id);
    }

    bool client_exists(const std::string& client_id) const override {
        return clients_.contains(client_id);
    }

    std::size_t notebook_count_for_client(const std::string& client_id) const override {
        std::size_t count = 0;
        for (const auto& [_, notebook] : notebooks_) {
            if (notebook.anonymous_client_id == client_id) {
                ++count;
            }
        }
        return count;
    }

    std::size_t stored_bytes_for_client(const std::string& client_id) const override {
        std::size_t bytes = 0;
        for (const auto& [_, notebook] : notebooks_) {
            if (notebook.anonymous_client_id == client_id) {
                bytes += notebook.size_bytes;
            }
        }
        return bytes;
    }

    void create_notebook(const StoredNotebook& notebook) override {
        notebooks_.emplace(notebook.id, notebook);
    }

    std::vector<StoredNotebook> list_notebooks(const std::string& client_id) const override {
        std::vector<StoredNotebook> notebooks;
        for (const auto& [_, notebook] : notebooks_) {
            if (notebook.anonymous_client_id == client_id) {
                notebooks.push_back(notebook);
            }
        }
        std::sort(notebooks.begin(), notebooks.end(), [](const auto& left, const auto& right) {
            if (left.updated_at != right.updated_at) {
                return left.updated_at > right.updated_at;
            }
            return left.id < right.id;
        });
        return notebooks;
    }

    std::optional<StoredNotebook> get_notebook(const std::string& notebook_id) const override {
        const auto notebook = notebooks_.find(notebook_id);
        if (notebook == notebooks_.end()) {
            return std::nullopt;
        }
        return notebook->second;
    }

    void update_notebook(
        const std::string& notebook_id,
        const std::string& title,
        const std::string& document_json,
        const std::string& updated_at,
        std::size_t size_bytes) override {
        auto notebook = notebooks_.find(notebook_id);
        if (notebook == notebooks_.end()) {
            return;
        }
        notebook->second.title = title;
        notebook->second.document_json = document_json;
        notebook->second.updated_at = updated_at;
        notebook->second.size_bytes = size_bytes;
    }

    void touch_notebook(const std::string& notebook_id, const std::string& opened_at) override {
        auto notebook = notebooks_.find(notebook_id);
        if (notebook != notebooks_.end()) {
            notebook->second.last_opened_at = opened_at;
        }
    }

    void delete_notebook(const std::string& notebook_id) override {
        notebooks_.erase(notebook_id);
    }

private:
    std::set<std::string> clients_;
    std::map<std::string, StoredNotebook> notebooks_;
};

}  // namespace

std::unique_ptr<NotebookStore> make_memory_notebook_store() {
    return std::make_unique<MemoryNotebookStore>();
}

}  // namespace aleph3::web
