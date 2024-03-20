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
        // Lock the memory mapped file into memory.
        //
        // This increases open time and persistent memory usage, but decreases
        // latency when searching for paths. This should only be used for
        // long-running processes, like the websearch app.
        bool lock_into_memory = false;
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
