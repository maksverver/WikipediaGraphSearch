#ifndef WIKIPATH_GRAPH_READER_H_INCLUDED
#define WIKIPATH_GRAPH_READER_H_INCLUDED

#include "common.h"

#include <stdint.h>

#include <memory>
#include <span>

namespace wikipath {

class EdgeList {
public:
    class Iterator {
    public:
        using value_type = index_t;
        using difference_type = std::ptrdiff_t;
        using pointer = const index_t*;
        using reference = const index_t&;
        using iterator_category = std::forward_iterator_tag;

        Iterator() : p(nullptr) {}
        Iterator(const Iterator&) = default;
        Iterator& operator=(const Iterator&) = default;
        Iterator& operator++() { ++p; return *this; }
        Iterator operator++(int) { return Iterator(p++); }
        value_type operator*() const { return *p; }
        bool operator==(const Iterator &it) const { return p == it.p || (AtEnd() && it.AtEnd()); }
        bool operator!=(const Iterator &it) const { return !(*this == it); }

    private:
        friend class EdgeList;
        Iterator(const uint32_t *p) : p(p) {}
        bool AtEnd() const { return p == nullptr || *p == 0; }
        const uint32_t *p;
    };

    Iterator begin() const { return Iterator(p); }
    Iterator end() const { return Iterator(); }

private:
    friend class GraphReader;
    EdgeList(const uint32_t *p) : p(p) {}
    const uint32_t *const p;
};

// Accessor for the graph data structure. This class is thread-safe.
//
// Note: for performance reasons, no validation of the graph file data is done.
// If the file is corrupt, we may crash (or worse!)
class GraphReader {
public:
    using edges_t = std::span<const index_t>;

    ~GraphReader();

    GraphReader(const GraphReader&) = delete;
    GraphReader &operator=(const GraphReader&) = delete;

    struct OpenOptions {
        enum class MLock {
            // Do not lock anything into memory. Pages are swapped in on demand
            // and may be swapped out at the OS's discretion.
            //
            // In this mode, Open() is fast, but queries may be slow because
            // pages are loaded on-demand. Use this for one-off queries, to avoid
            // loading more data than necessary.
            NONE,

            // Lock pages into memory in the foreground. Open() does not return
            // until all pages are locked into memory, or until mlock() fails in
            // which case Open() fails too.
            //
            // In this mode, Open() is slow, but when it returns, queries are
            // fast. Use this only for long-running processes, where query
            // performance is more important than startup latency.
            FOREGROUND,

            // Lock pages into memory in a background thread. Open() will return
            // immediately. mlock() failure is ignored, and causes the reader to
            // behave as with LOCK_NONE.
            //
            // This mode is a compromise between NONE and FOREGROUND: Open() is
            // fast, and initial queries may be slow, but eventually the whole
            // file is mapped into memory and subsequent queries are fast.
            BACKGROUND,

            // Add the MAP_POPULATE flag to mmap(). Like FOREGROUND this blocks
            // the call to Open(), but failure to populate pages does not cause
            // Open() to fail, and there is no guarantee that pages will remain
            // locked into memory.
            POPULATE,
        };
        MLock mlock = MLock::NONE;
    };

    static std::unique_ptr<GraphReader> Open(const char *filename, OpenOptions options);

    // Precondition: i is between 0 and VertexCount() (exclusive)
    edges_t ForwardEdges(index_t i) const { return forward_edges.Edges(i); }
    edges_t BackwardEdges(index_t i) const { return backward_edges.Edges(i); }

    // Number of vertices, including 0.
    index_t VertexCount() const { return vertex_count; }

    // Number of edges (in one direction only; i.e. the forward and backward edges
    // combined are twice this number).
    index_t EdgeCount() const { return edge_count; }

    // Possible future improvement: expose the live status of MLock via an atomic
    // variable. Status could be one of: NONE, FOREGROUND_COMPLETED,
    // BACKGROUND_IN_PROGRESS, BACKGROUND_COMPLETED, BACKGROUND_FAILED.

private:
    GraphReader(void *data, size_t data_len);

    static_assert(std::is_same<index_t, uint32_t>::value);

    struct edges_index_t {
        const uint32_t *index;
        const uint32_t *edges;

        edges_t Edges(index_t i) const {
            return edges_t(&edges[index[i]], &edges[index[i + 1]]);
        }
    };

    edges_index_t forward_edges;
    edges_index_t backward_edges;
    uint32_t vertex_count;
    uint32_t edge_count;
    void *data;
    size_t data_len;
};

}  // namespace wikipath

#endif  // ndef WIKIPATH_GRAPH_READER_H_INCLUDED
