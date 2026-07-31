#include "web/PostgresNotebookStore.hpp"

#include <libpq-fe.h>

#include <initializer_list>
#include <limits>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace aleph3::web {

namespace {

[[noreturn]] void throw_postgres(PGconn* connection, std::string code, std::string message) {
    if (connection != nullptr) {
        message += ": ";
        message += PQerrorMessage(connection);
    }
    throw NotebookStoreError(std::move(code), std::move(message));
}

struct ResultDeleter {
    void operator()(PGresult* result) const noexcept {
        if (result != nullptr) {
            PQclear(result);
        }
    }
};

using ResultPtr = std::unique_ptr<PGresult, ResultDeleter>;

class PostgresNotebookStore final : public NotebookStore {
public:
    explicit PostgresNotebookStore(const std::string& connection_string)
        : connection_(PQconnectdb(connection_string.c_str())) {
        if (connection_ == nullptr || PQstatus(connection_) != CONNECTION_OK) {
            throw_postgres(connection_, "web.notebook_store_open_failed", "Unable to connect to Postgres notebook database");
        }
        initialize_schema();
    }

    ~PostgresNotebookStore() override {
        if (connection_ != nullptr) {
            PQfinish(connection_);
        }
    }

    void create_client(const std::string& client_id, const std::string& created_at) override {
        exec_params(
            "INSERT INTO anonymous_clients(id, created_at) VALUES($1, to_timestamp($2::double precision / 1000.0)) "
            "ON CONFLICT (id) DO NOTHING",
            {client_id, created_at},
            "web.client_store_failed",
            "Unable to persist anonymous client");
    }

    bool client_exists(const std::string& client_id) const override {
        const auto result = exec_params(
            "SELECT 1 FROM anonymous_clients WHERE id = $1 LIMIT 1",
            {client_id},
            "web.notebook_store_read_failed",
            "Unable to read anonymous client");
        return PQntuples(result.get()) == 1;
    }

    std::size_t notebook_count_for_client(const std::string& client_id) const override {
        const auto result = exec_params(
            "SELECT COUNT(*) FROM notebooks WHERE anonymous_client_id = $1",
            {client_id},
            "web.notebook_store_read_failed",
            "Unable to count client notebooks");
        return parse_size(PQgetvalue(result.get(), 0, 0));
    }

    std::size_t stored_bytes_for_client(const std::string& client_id) const override {
        const auto result = exec_params(
            "SELECT COALESCE(SUM(size_bytes), 0) FROM notebooks WHERE anonymous_client_id = $1",
            {client_id},
            "web.notebook_store_read_failed",
            "Unable to read client storage usage");
        return parse_size(PQgetvalue(result.get(), 0, 0));
    }

    void create_notebook(const StoredNotebook& notebook) override {
        exec_params(
            "INSERT INTO notebooks("
            "id, anonymous_client_id, title, document_json, created_at, updated_at, last_opened_at, size_bytes"
            ") VALUES($1, $2, $3, $4::jsonb, to_timestamp($5::double precision / 1000.0), "
            "to_timestamp($6::double precision / 1000.0), to_timestamp($7::double precision / 1000.0), $8::bigint)",
            {notebook.id,
                notebook.anonymous_client_id,
                notebook.title,
                notebook.document_json,
                notebook.created_at,
                notebook.updated_at,
                notebook.last_opened_at,
                std::to_string(notebook.size_bytes)},
            "web.notebook_store_write_failed",
            "Unable to create notebook");
    }

    std::vector<StoredNotebook> list_notebooks(const std::string& client_id) const override {
        const auto result = exec_params(
            "SELECT id, anonymous_client_id, title, document_json::text, "
            "round(extract(epoch from created_at) * 1000)::text, "
            "round(extract(epoch from updated_at) * 1000)::text, "
            "round(extract(epoch from last_opened_at) * 1000)::text, size_bytes::text "
            "FROM notebooks WHERE anonymous_client_id = $1 ORDER BY updated_at DESC, id ASC",
            {client_id},
            "web.notebook_store_read_failed",
            "Unable to list notebooks");
        return read_notebooks(result.get());
    }

    std::optional<StoredNotebook> get_notebook(const std::string& notebook_id) const override {
        const auto result = exec_params(
            "SELECT id, anonymous_client_id, title, document_json::text, "
            "round(extract(epoch from created_at) * 1000)::text, "
            "round(extract(epoch from updated_at) * 1000)::text, "
            "round(extract(epoch from last_opened_at) * 1000)::text, size_bytes::text "
            "FROM notebooks WHERE id = $1 LIMIT 1",
            {notebook_id},
            "web.notebook_store_read_failed",
            "Unable to read notebook");
        if (PQntuples(result.get()) == 0) {
            return std::nullopt;
        }
        return read_notebook(result.get(), 0);
    }

    void update_notebook(
        const std::string& notebook_id,
        const std::string& title,
        const std::string& document_json,
        const std::string& updated_at,
        std::size_t size_bytes) override {
        exec_params(
            "UPDATE notebooks SET title = $1, document_json = $2::jsonb, "
            "updated_at = to_timestamp($3::double precision / 1000.0), size_bytes = $4::bigint WHERE id = $5",
            {title, document_json, updated_at, std::to_string(size_bytes), notebook_id},
            "web.notebook_store_write_failed",
            "Unable to update notebook");
    }

    void touch_notebook(const std::string& notebook_id, const std::string& opened_at) override {
        exec_params(
            "UPDATE notebooks SET last_opened_at = to_timestamp($1::double precision / 1000.0) WHERE id = $2",
            {opened_at, notebook_id},
            "web.notebook_store_write_failed",
            "Unable to update notebook open time");
    }

    void delete_notebook(const std::string& notebook_id) override {
        exec_params(
            "DELETE FROM notebooks WHERE id = $1",
            {notebook_id},
            "web.notebook_store_write_failed",
            "Unable to delete notebook");
    }

private:
    static std::size_t parse_size(std::string_view text) {
        std::size_t value = 0;
        for (const char ch : text) {
            if (ch < '0' || ch > '9') {
                throw NotebookStoreError("web.notebook_store_read_failed", "Postgres returned a non-integer size value.");
            }
            const auto digit = static_cast<std::size_t>(ch - '0');
            if (value > (std::numeric_limits<std::size_t>::max() - digit) / 10u) {
                throw NotebookStoreError("web.notebook_store_value_too_large", "Postgres size value exceeds platform range.");
            }
            value = value * 10u + digit;
        }
        return value;
    }

    static StoredNotebook read_notebook(PGresult* result, int row) {
        return {
            PQgetvalue(result, row, 0),
            PQgetvalue(result, row, 1),
            PQgetvalue(result, row, 2),
            PQgetvalue(result, row, 3),
            PQgetvalue(result, row, 4),
            PQgetvalue(result, row, 5),
            PQgetvalue(result, row, 6),
            parse_size(PQgetvalue(result, row, 7))};
    }

    static std::vector<StoredNotebook> read_notebooks(PGresult* result) {
        std::vector<StoredNotebook> notebooks;
        for (int row = 0; row < PQntuples(result); ++row) {
            notebooks.push_back(read_notebook(result, row));
        }
        return notebooks;
    }

    void initialize_schema() {
        exec(
            "CREATE TABLE IF NOT EXISTS anonymous_clients ("
            "id text PRIMARY KEY,"
            "created_at timestamptz NOT NULL"
            ")");
        exec(
            "CREATE TABLE IF NOT EXISTS notebooks ("
            "id text PRIMARY KEY,"
            "anonymous_client_id text NOT NULL REFERENCES anonymous_clients(id) ON DELETE CASCADE,"
            "title text NOT NULL,"
            "document_json jsonb NOT NULL,"
            "created_at timestamptz NOT NULL,"
            "updated_at timestamptz NOT NULL,"
            "last_opened_at timestamptz NOT NULL,"
            "size_bytes bigint NOT NULL CHECK(size_bytes >= 0)"
            ")");
        exec(
            "CREATE INDEX IF NOT EXISTS notebooks_client_updated_idx "
            "ON notebooks(anonymous_client_id, updated_at DESC)");
    }

    void exec(const char* sql) {
        ResultPtr result(PQexec(connection_, sql));
        if (PQresultStatus(result.get()) != PGRES_COMMAND_OK) {
            throw_postgres(connection_, "web.notebook_store_init_failed", "Unable to initialize Postgres notebook schema");
        }
    }

    ResultPtr exec_params(
        const char* sql,
        std::initializer_list<std::string> parameters,
        const char* code,
        const char* message) const {
        std::vector<const char*> values;
        values.reserve(parameters.size());
        for (const auto& parameter : parameters) {
            values.push_back(parameter.c_str());
        }
        ResultPtr result(PQexecParams(
            connection_,
            sql,
            static_cast<int>(values.size()),
            nullptr,
            values.data(),
            nullptr,
            nullptr,
            0));
        const auto status = PQresultStatus(result.get());
        if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
            throw_postgres(connection_, code, message);
        }
        return result;
    }

    PGconn* connection_ = nullptr;
};

}  // namespace

std::unique_ptr<NotebookStore> make_postgres_notebook_store(const std::string& connection_string) {
    return std::make_unique<PostgresNotebookStore>(connection_string);
}

}  // namespace aleph3::web
