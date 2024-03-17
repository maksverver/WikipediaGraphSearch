#ifndef WIKIPATH_ANNOTATED_DAG_H_INCLUDED
#define WIKIPATH_ANNOTATED_DAG_H_INCLUDED

#include "wikipath/common.h"
#include "wikipath/reader.h"

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

// A link between two pages: from Src() to Dst(), displayed as Text().
//
// This class is thread-compatible, but not thread safe: the same instance
// should not be accessed concurrently from multiple threads.
class AnnotatedLink {
public:
    AnnotatedLink(AnnotatedPage *src, AnnotatedPage *dst, const Reader *reader)
            : src(src), dst(dst), reader_or_text(reader) {}

    // Movable but not copyable.
    AnnotatedLink(const AnnotatedLink &) = delete;
    AnnotatedLink &operator=(const AnnotatedLink &) = delete;
    AnnotatedLink(AnnotatedLink &&) = default;
    AnnotatedLink &operator=(AnnotatedLink &&) = default;

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

enum class LinkOrder : char {
    ID,     // order links by target page id (default)
    TITLE,  // order links by target page title
    TEXT,   // order links by link text
};

static constexpr LinkOrder DEFAULT_LINK_ORDER = LinkOrder::ID;

// A page in the DAG with an Id() and Title().
//
// This class is thread-compatible, but not thread safe: the same instance
// should not be accessed concurrently from multiple threads.
class AnnotatedPage {
public:
    AnnotatedPage(index_t id, const Reader *reader)
            : id(id), reader_or_title(reader) {}

    // Movable but not copyable.
    AnnotatedPage(const AnnotatedPage &) = delete;
    AnnotatedPage &operator=(const AnnotatedPage &) = delete;
    AnnotatedPage(AnnotatedPage &&) = default;
    AnnotatedPage &operator=(AnnotatedPage &&) = default;

    index_t Id() const { return id; }

    std::string_view Title() const;

    std::string Ref() const { return PageRef(id, Title()); }

    // Returns the outgoing links for this page within the DAG, in the given order.
    std::span<const AnnotatedLink> Links(LinkOrder order = DEFAULT_LINK_ORDER) const {
        if (links_order != order) {
            SortLinks(links, order);
            links_order = order;
        }
        return links;
    }

private:
    void AddLink(AnnotatedPage *dst, const Reader *reader) {
        links.push_back(AnnotatedLink(this, dst, reader));
    }

    int64_t PathCount(const AnnotatedPage *finish) const {
        if (path_count == -1) path_count = CalculatePathCount(this, finish);
        return path_count;
    }

    static int64_t CalculatePathCount(const AnnotatedPage *page, const AnnotatedPage *finish);
    static void SortLinks(std::vector<AnnotatedLink> &links, LinkOrder order);

    friend class AnnotatedDag;
    friend class PathEnumerator;

    index_t id;
    mutable std::variant<const Reader*, std::string> reader_or_title;
    mutable std::vector<AnnotatedLink> links;
    mutable int64_t path_count = -1;
    mutable LinkOrder links_order = LinkOrder::ID;
};

// Represents a DAG, like produced by FindShortestPathDag(), annotated with
// page titles and link text. The metadata is loaded on-demand using the given
// reader.
//
// Supports:
//
//   - efficient computation of the total path count
//   - efficient enumeration of paths starting at an arbitrary offset
//   - efficient enumeration of paths in lexicographical order
//
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

    // Movable but not copyable.
    AnnotatedDag(const AnnotatedDag&) = delete;
    AnnotatedDag& operator=(const AnnotatedDag&) = delete;
    AnnotatedDag(AnnotatedDag&&) = default;
    AnnotatedDag& operator=(AnnotatedDag&&) = default;

    const AnnotatedPage *Start() const { return start; }
    const AnnotatedPage *Finish() const { return finish; }

    // Returns a count of the number of paths from Start() to Finish(), without
    // explicitly enumerating all possible paths.
    int64_t CountPaths() const;

    using path_t = std::span<const AnnotatedLink *const>;
    using enumerate_paths_callback_t = std::function<bool(path_t)>;

    // Enumerate paths from Start() to Finish(), starting from the given 0-based
    // offset.
    //
    // For each path found, the callback is called with a list of edges in the
    // path, until the callback returns `false` or all paths have been
    // enumerated, whichever comes first. The function itself returns `false`
    // if the callback ever returned `false`, or `true` otherwise, including in
    // the case were no paths were found so the callback was never called.
    //
    // The implementation doesn't copy the spans returned by AnnotatedPage::Links(),
    // which means it has a weakness: the callback function itself is not allowed
    // to call Links() or EnumeratePaths() with a different `order` value!
    //
    // If this is a concern, then you should use PathEnumerator, declared below,
    // instead.
    bool EnumeratePaths(
            enumerate_paths_callback_t callback,
            int64_t offset = 0, LinkOrder order = DEFAULT_LINK_ORDER) const;

private:
    struct EnumeratePathsContext {
        const AnnotatedPage *finish;
        enumerate_paths_callback_t callback;
        int64_t offset;
        LinkOrder order;
        std::vector<const AnnotatedLink*> links;

        bool EnumeratePaths(const AnnotatedPage *page);
    };

    const Reader *reader;
    const AnnotatedPage *start;
    const AnnotatedPage *finish;
    std::vector<AnnotatedPage> pages;
};

// Enumerates paths through the DAG in the given LinkOrder.
//
// Example of basic usage:
//
//  for (PathEnumerator enumerator(dag); enumerator.HasPath(); enumerator.Advance()) {
//      auto path = enumerator.Path();
//      for (const AnnotatedLink *link : path) { /* .. */ }
//  }
//
// This class has a few benefits compared to AnnotatedDag::EnumeratePaths():
//
//   - This class does not take a callback function, but instead, allows
//     the caller to retrieve the current path with Path(), and advance to
//     the next path with Advance().
//
//   - The Advance() method allows efficiently skipping paths.
//
//   - This class copies the links onto a stack, which means it's safe to
//     call AnnotatedPage::Links() with a different order on any of the pages,
//     unlike EnumeratePaths().
//
//   - Instances are copyable, though not in constant time.
//
// Compared to EnumeratePaths(), this class might be faster to enumerate all
// paths, though it might be slower to find only the first path, because it
// still pushes links to be visited later onto the stack.
class PathEnumerator {
public:
    // Movable and copyable.
    PathEnumerator(const PathEnumerator&) = default;
    PathEnumerator& operator=(const PathEnumerator&) = default;
    PathEnumerator(PathEnumerator&&) = default;
    PathEnumerator& operator=(PathEnumerator&&) = default;

    // Create a PathEnumerator that skips the first `skip` paths.
    PathEnumerator(const AnnotatedDag &dag, int64_t skip = 0, LinkOrder order = DEFAULT_LINK_ORDER)
        : finish(dag.Finish()), order(order) {
        const AnnotatedPage *start = dag.Start();
        has_path = start == finish ? skip == 0 : FindPathToFinish(start, skip);
    }

    using path_t = AnnotatedDag::path_t;
    path_t Path() const { return path; }
    bool HasPath() const { return has_path; }
    operator bool() const { return has_path; }

    // Advances to the next path, after skipping the next `skip` paths.
    void Advance(int64_t skip = 0) {
        const AnnotatedPage *page = AdvanceToNextPage(skip);
        has_path = page != nullptr && FindPathToFinish(page, skip);
    }

private:
    const AnnotatedPage *AdvanceToNextPage(int64_t &skip);

    bool FindPathToFinish(const AnnotatedPage *page, int64_t skip);

    bool has_path;
    const AnnotatedPage *finish;
    LinkOrder order;
    std::vector<const AnnotatedLink*> path;
    std::vector<const AnnotatedLink*> stack;
};

}  // namespace wikipath

#endif // ndef WIKIPATH_ANNOTATED_DAG_H_INCLUDED
