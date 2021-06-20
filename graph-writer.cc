#include "common.h"

#include "graph-header.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <vector>

namespace {

int64_t CountEdges(const std::vector<std::vector<index_t>> &edgelist) {
    int64_t edge_count = 0;
    for (const auto &adj : edgelist) edge_count += adj.size();
    return edge_count;
}

int64_t GetEdgeListSize(const std::vector<std::vector<index_t>> &edgelist) {
    int64_t size = 0;
    for (const auto &adj : edgelist) {
        if (!adj.empty()) size += adj.size() + 1;
    }
    return size;
}

bool WriteInt(FILE *fp, uint32_t i) {
    return fwrite(&i, 4, 1, fp) == 1;
}

bool WriteInt(FILE *fp, int64_t i) {
    assert(i >= 0 && i <= 0xffffffff);
    return WriteInt(fp, (uint32_t) i);
}

bool WriteEdgeIndex(FILE *fp, int64_t offset, const std::vector<std::vector<index_t>> &edgelist) {
    assert(offset > 0);
    for (const auto &adj : edgelist) {
        if (adj.empty()) {
            if (!WriteInt(fp, 0u)) return false;
        } else {
            if (!WriteInt(fp, offset)) return false;
            offset += (int64_t) adj.size() + 1;
        }
    }
    return true;
}

bool WriteEdgeList(FILE *fp, const std::vector<std::vector<index_t>> &edgelist) {
    for (const auto &adj : edgelist) {
        if (!adj.empty()) {
            for (index_t i : adj) {
                assert(i > 0);
                if (!WriteInt(fp, i)) return false;
            }
            if (!WriteInt(fp, 0u)) return false;
        }
    }
    return true;
}

bool WriteGraphOutput(FILE *fp,
        const std::vector<std::vector<index_t>> &forward_edges,
        const std::vector<std::vector<index_t>> &backward_edges) {
    const int64_t vertex_count = forward_edges.size();  // includes vertex 0!
    const int64_t edge_count = CountEdges(forward_edges);
    const int64_t forward_edge_index_size = vertex_count;
    const int64_t backward_edge_index_size = vertex_count;
    const int64_t forward_edge_list_size = GetEdgeListSize(forward_edges);
    const int64_t backward_edge_list_size = GetEdgeListSize(backward_edges);

    const int64_t forward_edge_index_offset = GRAPH_HEADER_FIELD_COUNT;
    const int64_t backward_edge_index_offset = forward_edge_index_offset + forward_edge_index_size;
    const int64_t forward_edge_list_offset = backward_edge_index_offset + backward_edge_index_size;
    const int64_t backward_edge_list_offset = forward_edge_list_offset + forward_edge_list_size;
    const int64_t file_size = backward_edge_list_offset + backward_edge_list_size;

    // Write header.
    // An exquisite application of the for-case paradigm!
    // See: https://thedailywtf.com/articles/The_FOR-CASE_paradigm
    for (int i = 0; i < GRAPH_HEADER_FIELD_COUNT; ++i) {
        switch (i) {
        case GRAPH_HEADER_MAGIC:
            if (!WriteInt(fp, graph_header_magic_value)) return false;
            break;
        case GRAPH_HEADER_FILE_SIZE:
            if (!WriteInt(fp, file_size)) return false;
            break;
        case GRAPH_HEADER_VERTEX_COUNT:
            if (!WriteInt(fp, vertex_count)) return false;
            break;
        case GRAPH_HEADER_EDGE_COUNT:
            if (!WriteInt(fp, edge_count)) return false;
            break;
        case GRAPH_HEADER_FORWARD_EDGE_INDEX_OFFSET:
            if (!WriteInt(fp, forward_edge_index_offset)) return false;
            break;
        case GRAPH_HEADER_BACKWARD_EDGE_INDEX_OFFSET:
            if (!WriteInt(fp, backward_edge_index_offset)) return false;
            break;
        default:
            abort();
        }
    }

    // Edge data
    if (!WriteEdgeIndex(fp, forward_edge_list_offset, forward_edges)) return false;
    if (!WriteEdgeIndex(fp, backward_edge_list_offset, backward_edges)) return false;
    if (!WriteEdgeList(fp, forward_edges)) return false;
    if (!WriteEdgeList(fp, backward_edges)) return false;

    return true;
}

}  // namespace

bool WriteGraphOutput(
        const char *filename,
        const std::vector<std::vector<index_t>> &outlinks,
        const std::vector<std::vector<index_t>> &inlinks) {
    assert(inlinks.size() == outlinks.size());
    FILE *fp = fopen(filename, "wb");
    if (fp == nullptr) return false;
    bool success = WriteGraphOutput(fp, outlinks, inlinks);
    if (fclose(fp) != 0) success = false;
    return success;
}
