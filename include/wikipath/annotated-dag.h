#ifndef WIKIPATH_ANNOTATED_DAG_H_INCLUDED
#define WIKIPATH_ANNOTATED_DAG_H_INCLUDED

#include "wikipath/common.h"
#include "wikipath/metadata-reader.h"
#include "wikipath/reader.h"

#include <cassert>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace wikipath {

class AnnotatedPage;

class AnnotatedLink {
public:
    AnnotatedLink(AnnotatedPage *src, AnnotatedPage *dst, const Reader *reader)
            : src(src), dst(dst), reader_or_text(reader) {}

    AnnotatedPage *Src() { return src; }
    AnnotatedPage *Dst() { return dst; }

    const AnnotatedPage *Src() const { return src; }
    const AnnotatedPage *Dst() const { return dst; }

    std::string_view Text() const;

    std::string ForwardRef() const;
    std::string BackwardRef() const;

private:
    AnnotatedPage *src;
    AnnotatedPage *dst;
    mutable std::variant<const Reader*, std::string> reader_or_text;
};

class AnnotatedPage {
public:
    AnnotatedPage(index_t id, const Reader *reader)
            : id(id), reader_or_title(reader) {}

    index_t Id() const { return id; }

    std::string_view Title() const;

    std::string Ref() const { return PageRef(id, Title()); }

    std::span<AnnotatedLink> Adj() { return adj; }
    std::span<const AnnotatedLink> Adj() const { return adj; }

private:
    void AddLink(AnnotatedPage *dst, const Reader *reader) {
        adj.push_back(AnnotatedLink(this, dst, reader));
    }

    int64_t PathCount(const AnnotatedPage *finish) const {
        if (path_count == -1) path_count = CalculatePathCount(this, finish);
        return path_count;
    }

    static int64_t CalculatePathCount(const AnnotatedPage *page, const AnnotatedPage *finish);

    friend class AnnotatedDag;

    index_t id;
    mutable std::variant<const Reader*, std::string> reader_or_title;
    std::vector<AnnotatedLink> adj;
    mutable int64_t path_count = -1;
};

// Represents a DAG, like produced by FindShortestPathDag(), annotated with
// page titles and link textx. The metadata is loaded on-demand using the given
// reader.
class AnnotatedDag {
public:
    // Constructs an AnnotatedDag using the given reader from an edge list that
    // describes the page indices.
    //
    // Stores a copy of `reader` but does not take ownership! The caller must
    // ensure the reader instance stays valid for the lifetime of the AnnotatedDag.
    AnnotatedDag(
            const Reader *reader, index_t start_id, index_t finish_id,
            const std::vector<std::pair<index_t, index_t>> &edge_list);

    // Returns a count of the number of paths from Start() to Finish(), without
    // explicitly enumerating all possible paths.
    int64_t CountPaths();

    using Path = std::span<const AnnotatedLink*>;
    using EnumeratePathsCallback = std::function<bool(Path)>;

    // Enumerate paths from Start() to Finish(), starting from the given 0-based
    // offset.
    //
    // For each path found, the callback is called with a list of edges in the
    // path, until the callback returns `false` or all paths have been
    // enumerated, whichever comes first. The function itself returns `false`
    // if the callback ever returned `false`, or `true` otherwise, including in
    // the case were no paths were found so the callback was never called.
    bool EnumeratePaths(
            const EnumeratePathsCallback &callback,
            size_t offset = 0) const;

    const AnnotatedPage *Start() const { return start; }
    const AnnotatedPage *Finish() const { return finish; }

private:
    struct EnumeratePathsContext {
        const AnnotatedPage *finish;
        const EnumeratePathsCallback *callback;
        size_t offset;
        std::vector<const AnnotatedLink*> links;

        bool EnumeratePaths(const AnnotatedPage *page);
    };

    const Reader *reader;
    const AnnotatedPage *start;
    const AnnotatedPage *finish;
    std::vector<AnnotatedPage> pages;
};

}  // namespace wikipath

#endif // ndef WIKIPATH_ANNOTATED_DAG_H_INCLUDED
