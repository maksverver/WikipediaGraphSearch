#ifndef METADATA_WRITER_H_INCLUDED
#define METADATA_WRITER_H_INCLUDED

#include "common.h"

#include <sqlite3.h>

#include <optional>
#include <string>
#include <memory>

class MetadataWriter {
public:
    static std::unique_ptr<MetadataWriter> Create(const char *filename);

    bool InsertPage(index_t page_id, const std::string &title);
    bool InsertLink(index_t from_page_id, index_t to_page_id, const std::optional<std::string> &title);

    ~MetadataWriter();

private:
    MetadataWriter(sqlite3 *db);

    bool Init();
    bool Execute(const char *sql);
    bool Prepare(sqlite3_stmt **stmt, const char *sql);

    sqlite3 *const db;
    sqlite3_stmt *insert_page_stmt = nullptr;
    sqlite3_stmt *insert_link_stmt = nullptr;
};

#endif  // ndef METADATA_WRITER_H_INCLUDED
