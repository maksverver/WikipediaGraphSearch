#ifndef GRAPH_WRITER_H_INCLUDED
#define GRAPH_WRITER_H_INCLUDED

#include "common.h"

#include <vector>

bool WriteGraphOutput(
        const char *filename,
        const std::vector<std::vector<index_t>> &outlinks,
        const std::vector<std::vector<index_t>> &inlinks);

#endif  // ndef GRAPH_WRITER_H_INCLUDED
