#ifndef WIKIPATH_GRAPH_WRITER_H_INCLUDED
#define WIKIPATH_GRAPH_WRITER_H_INCLUDED

#include "common.h"

#include <vector>

namespace wikipath {

bool WriteGraphOutput(
        const char *filename,
        const std::vector<std::vector<index_t>> &outlinks,
        const std::vector<std::vector<index_t>> &inlinks);

}  // namespace wikipath

#endif  // ndef WIKIPATH_GRAPH_WRITER_H_INCLUDED
