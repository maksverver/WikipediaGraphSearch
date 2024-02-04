#ifndef WIKIPATH_METADATA_READER_H_INCLUDED
#define WIKIPATH_METADATA_READER_H_INCLUDED

#include "common.h"

#include <sqlite3.h>

#include <optional>
#include <memory>
#include <mutex>
#include <string>

namespace wikipath {

// Accessor for the graph metadata database. This class is thread-safe.
class MetadataReader {
public:
    static std::unique_ptr<MetadataReader> Open(const char *filename);

    struct Page {
        index_t id = 0;
        std::string title;

        bool operator==(const Page&) const = default;

        // Hash code. Only used by Python bindings.
        size_t Hash() const noexcept {
            size_t hash = 0;
            hash = 8191*hash + id;
            hash = 8191*hash + std::hash<decltype(title)>{}(title);
            return hash;
        }
    };

    struct Link {
        index_t from_page_id = 0;
        index_t to_page_id = 0;
        std::optional<std::string> title;

        bool operator==(const Link&) const = default;

        // Hash code. Only used by Python bindings.
        size_t Hash() const noexcept {
            size_t hash = 0;
            hash = 8191*hash + from_page_id;
            hash = 8191*hash + to_page_id;
            hash = 8191*hash + std::hash<decltype(title)>{}(title);
            return hash;
        }
    };

    std::optional<Page> GetPageById(index_t id) const;
    std::optional<Page> GetPageByTitle(const std::string &title) const;
    std::optional<Link> GetLink(index_t from_page_id, index_t to_page_id) const;

    ~MetadataReader();

private:
    MetadataReader(sqlite3 *db);

    bool Init();
    bool Prepare(sqlite3_stmt **stmt, const char *sql);
    std::optional<Page> GetPage(sqlite3_stmt *stmt) const;

    mutable std::mutex mutex;
    sqlite3 *const db;
    sqlite3_stmt *get_page_by_id_stmt = nullptr;
    sqlite3_stmt *get_page_by_title_stmt = nullptr;
    sqlite3_stmt *get_link_stmt = nullptr;
};

}  // namespace wikipath

template<> struct std::hash<wikipath::MetadataReader::Page> {
  size_t operator()(const wikipath::MetadataReader::Page& page) const noexcept {
    return page.Hash();
  }
};

template<> struct std::hash<wikipath::MetadataReader::Link> {
  size_t operator()(const wikipath::MetadataReader::Link& link) const noexcept {
    return link.Hash();
  }
};

#endif  // ndef WIKIPATH_METADATA_READER_H_INCLUDED
