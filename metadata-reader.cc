#include "metadata-reader.h"

#include <assert.h>
#include <iostream>

namespace {

constexpr const char *get_page_by_id_sql = "SELECT page_id, title FROM pages WHERE page_id = ?";
constexpr const char *get_page_by_title_sql = "SELECT page_id, title FROM pages WHERE title = ?";
constexpr const char *get_link_sql = "SELECT from_page_id, to_page_id, title FROM links WHERE from_page_id = ? AND to_page_id = ?";

std::optional<std::string> ColumnTextAsString(sqlite3_stmt *stmt, int column) {
    const char *text = (const char*) sqlite3_column_text(stmt, column);
    if (text == nullptr) return std::nullopt;
    return std::string(text, sqlite3_column_bytes(stmt, column));
}

}  // namespace

MetadataReader::MetadataReader(sqlite3 *db) : db(db) {}

MetadataReader::~MetadataReader() {
    // Note: sqlite3_finalize is safe to call with a NULL argument.
    sqlite3_finalize(get_page_by_id_stmt);
    sqlite3_finalize(get_page_by_title_stmt);
    sqlite3_finalize(get_link_stmt);
    if (sqlite3_close(db) != SQLITE_OK) {
        std::cerr << "Failed to close database! " << sqlite3_errmsg(db) << std::endl;
    }
}

std::optional<MetadataReader::Page> MetadataReader::GetPage(sqlite3_stmt *stmt) {
    std::optional<MetadataReader::Page> result;
    int status = sqlite3_step(stmt);
    if (status == SQLITE_ROW) {
        result = Page{
            .id = (index_t) sqlite3_column_int64(stmt, 0),
            .title = ColumnTextAsString(stmt, 1).value(),
        };
        assert(sqlite3_step(stmt) == SQLITE_DONE);
    } else if (status != SQLITE_DONE) {
        std::cerr << "Failed to retrieve page! " << sqlite3_errmsg(db) << std::endl;
    }
    return result;
}

std::optional<MetadataReader::Page> MetadataReader::GetPageById(index_t id) {
    sqlite3_stmt *stmt = get_page_by_id_stmt;
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, id);
    std::optional<MetadataReader::Page> result = GetPage(stmt);
    sqlite3_clear_bindings(stmt);
    return result;
}

std::optional<MetadataReader::Page> MetadataReader::GetPageByTitle(const std::string &title) {
    sqlite3_stmt *stmt = get_page_by_title_stmt;
    sqlite3_reset(stmt);
    sqlite3_bind_text(stmt, 1, title.data(), title.size(), nullptr);
    std::optional<MetadataReader::Page> result = GetPage(stmt);
    sqlite3_clear_bindings(stmt);
    return result;
}

std::optional<MetadataReader::Link> MetadataReader::GetLink(index_t from_page_id, index_t to_page_id) {
    sqlite3_stmt *stmt = get_link_stmt;
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, from_page_id);
    sqlite3_bind_int64(stmt, 2, to_page_id);
    std::optional<MetadataReader::Link> result;
    int status = sqlite3_step(stmt);
    if (status == SQLITE_ROW) {
        result = Link {
            .from_page_id = (index_t) sqlite3_column_int64(stmt, 0),
            .to_page_id = (index_t) sqlite3_column_int64(stmt, 1),
            .title = ColumnTextAsString(stmt, 2),
        };
        assert(sqlite3_step(stmt) == SQLITE_DONE);
    } else if (status != SQLITE_DONE) {
        std::cerr << "Failed to retrieve page! " << sqlite3_errmsg(db) << std::endl;
    }
    return result;
}

bool MetadataReader::Prepare(sqlite3_stmt **stmt, const char *sql) {
    if (sqlite3_prepare_v2(db, sql, -1, stmt, NULL) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: [" << sql << "]: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    return true;
}

bool MetadataReader::Init() {
    if (!Prepare(&get_page_by_id_stmt, get_page_by_id_sql)) return false;
    if (!Prepare(&get_page_by_title_stmt, get_page_by_title_sql)) return false;
    if (!Prepare(&get_link_stmt, get_link_sql)) return false;
    return true;
}

std::unique_ptr<MetadataReader> MetadataReader::Open(const char *filename) {
    sqlite3 *db = nullptr;
    int status = sqlite3_open_v2(filename, &db, SQLITE_OPEN_READONLY, nullptr);
    if (status != SQLITE_OK) return nullptr;

    std::unique_ptr<MetadataReader> metadata_reader(new MetadataReader(db));
    if (!metadata_reader->Init()) return nullptr;
    return metadata_reader;
}
