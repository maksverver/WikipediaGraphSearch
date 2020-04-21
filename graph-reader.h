#ifndef GRAPH_READER_H_INCLUDED
#define GRAPH_READER_H_INCLUDED

#include "common.h"

#include <stdint.h>

#include <memory>

class EdgeList {
public:
    class Iterator {
    public:
        Iterator() : p(nullptr) {}
        Iterator(const Iterator&) = default;
        Iterator& operator=(const Iterator&) = default;
        Iterator& operator++() { ++p; return *this; }
        Iterator operator++(int) { return Iterator(p++); }
        index_t operator*() const { return *p; }
        bool operator==(const Iterator &it) const { return p == it.p || (AtEnd() && it.AtEnd()); }
        bool operator!=(const Iterator &it) const { return !(*this == it); }

    private:
        friend class EdgeList;
        Iterator(const uint32_t *p) : p(p) {}
        bool AtEnd() const { return p == nullptr || *p == 0; }
        const uint32_t *p;
    };

    Iterator begin() { return Iterator(p); }
    Iterator end() { return Iterator(); }

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
    ~GraphReader();

    GraphReader(const GraphReader&) = delete;
    GraphReader &operator=(const GraphReader&) = delete;

    static std::unique_ptr<GraphReader> Open(const char *filename);

    EdgeList ForwardEdges(index_t i) const { return EdgeListAt(forward_index[i]); }
    EdgeList BackwardEdges(index_t i) const { return EdgeListAt(backward_index[i]); }

    // Number of vertices, including 0. Valid vertex indices are between 1 and Size() (exclusive).
    index_t VertexCount() const { return vertex_count; }

    // Number of edges (in one direction only; i.e. the forward and backward edges
    // combined are twice this number).
    index_t EdgeCount() const { return edge_count; }

private:
    GraphReader(uint32_t *data, size_t data_len);
    EdgeList EdgeListAt(uint32_t offset) const { return EdgeList(offset > 0 ? &data[offset] : nullptr); }

    const uint32_t *const data;
    const size_t data_len;
    const uint32_t *const forward_index;
    const uint32_t *const backward_index;
    const uint32_t vertex_count;
    const uint32_t edge_count;
};

#endif  // ndef GRAPH_READER_H_INCLUDED
