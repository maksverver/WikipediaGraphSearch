#include "metadata-writer.h"

#include <assert.h>
#include <iostream>

constexpr const char *schema[] = {
R"(CREATE TABLE pages(
    page_id INTEGER NOT NULL PRIMARY KEY,
    title TEXT NOT NULL UNIQUE
))",
R"(CREATE TABLE links(
    from_page_id INTEGER NOT NULL REFERENCES pages(page_id),
    to_page_id INTEGER NOT NULL REFERENCES pages(page_id),
    title TEXT NULL,
    PRIMARY KEY(from_page_id, to_page_id)
) WITHOUT ROWID)",
"PRAGMA user_version = 1",
};

constexpr const char *insert_page_sql = "INSERT INTO pages(page_id, title) VALUES (?, ?)";
constexpr const char *insert_link_sql = "INSERT INTO links(from_page_id, to_page_id, title) VALUES (?, ?, ?)";

MetadataWriter::MetadataWriter(sqlite3 *db) : db(db) {}

MetadataWriter::~MetadataWriter() {
    Execute("END TRANSACTION");
    Execute("VACUUM");
    // Note: sqlite3_finalize is safe to call with a NULL argument.
    sqlite3_finalize(insert_page_stmt);
    sqlite3_finalize(insert_link_stmt);
    if (sqlite3_close(db) != SQLITE_OK) {
        std::cerr << "Failed to close database! " << sqlite3_errmsg(db) << std::endl;
    }
}

bool MetadataWriter::InsertPage(index_t page_id, const std::string &title) {
    sqlite3_stmt *stmt = insert_page_stmt;
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, page_id);
    sqlite3_bind_text(stmt, 2, title.data(), title.size(), nullptr);
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    if (!success) {
        std::cerr << "Failed to insert page! " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_clear_bindings(stmt);
    return success;
}

bool MetadataWriter::InsertLink(index_t from_page_id, index_t to_page_id, const std::optional<std::string> &title) {
    sqlite3_stmt *stmt = insert_link_stmt;
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, from_page_id);
    sqlite3_bind_int64(stmt, 2, to_page_id);
    sqlite3_bind_text(stmt, 3, title ? title->data() : nullptr, title ? title->size() : 0, nullptr);
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    if (!success) {
        std::cerr << "Failed to insert link! " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_clear_bindings(stmt);
    return success;
}

bool MetadataWriter::Execute(const char *sql) {
    int status = sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
    if (status != SQLITE_OK) {
        std::cerr << "Failed to execute SQL query " << sql << ": "
                << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    return true;
}

bool MetadataWriter::Prepare(sqlite3_stmt **stmt, const char *sql) {
    if (sqlite3_prepare_v2(db, sql, -1, stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: [" << sql << "]: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    return true;
}

bool MetadataWriter::Init() {
    // For maximum write performance, disable journaling. If any write fails, the
    // database will be corrupt, but that's okay.
    if (!Execute("PRAGMA journal_mode = off")) return false;
    if (!Execute("BEGIN EXCLUSIVE TRANSACTION")) return false;
    for (const char *sql : schema) {
        if (!Execute(sql)) return false;
    }
    if (!Prepare(&insert_page_stmt, insert_page_sql)) return false;
    if (!Prepare(&insert_link_stmt, insert_link_sql)) return false;
    return true;
}

std::unique_ptr<MetadataWriter> MetadataWriter::Create(const char *filename) {
    sqlite3 *db = nullptr;
    int status = sqlite3_open_v2(filename, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (status != SQLITE_OK) {
        std::cerr << "Could not create database file [" << filename << "]";
        return nullptr;
    }

    std::unique_ptr<MetadataWriter> metadata_writer(new MetadataWriter(db));
    if (!metadata_writer->Init()) return nullptr;
    return metadata_writer;
}
