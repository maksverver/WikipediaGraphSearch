#ifndef METADATA_READER_H_INCLUDED
#define METADATA_READER_H_INCLUDED

#include "common.h"

#include <sqlite3.h>

#include <optional>
#include <string>
#include <memory>

// Important: due to the reuse of prepared statements, this class is NOT thread-safe!
class MetadataReader {
public:
    static std::unique_ptr<MetadataReader> Open(const char *filename);

    struct Page {
        index_t id = 0;
        std::string title;
    };

    struct Link {
        index_t from_page_id = 0;
        index_t to_page_id = 0;
        std::optional<std::string> title;
    };

    std::optional<Page> GetPageById(index_t id);
    std::optional<Page> GetPageByTitle(const std::string &title);
    std::optional<Link> GetLink(index_t from_page_id, index_t to_page_id);

    ~MetadataReader();

private:
    MetadataReader(sqlite3 *db);

    bool Init();
    bool Prepare(sqlite3_stmt **stmt, const char *sql);
    std::optional<Page> GetPage(sqlite3_stmt *stmt);

    sqlite3 *const db;
    sqlite3_stmt *get_page_by_id_stmt = nullptr;
    sqlite3_stmt *get_page_by_title_stmt = nullptr;
    sqlite3_stmt *get_link_stmt = nullptr;
};

#endif  // ndef METADATA_READER_H_INCLUDED
