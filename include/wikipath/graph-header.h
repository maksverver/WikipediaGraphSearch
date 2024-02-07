#ifndef WIKIPATH_GRAPH_HEADER_H_INCLUDED
#define WIKIPATH_GRAPH_HEADER_H_INCLUDED

#include <stdint.h>

namespace wikipath {

const uint32_t graph_header_magic_value    = 0x68707247u;  // Grph
const uint32_t graph_header_reserved_value = 0;

enum GraphHeaderFields {
    GRAPH_HEADER_MAGIC,
    GRAPH_HEADER_RESERVED,
    GRAPH_HEADER_VERTEX_COUNT,
    GRAPH_HEADER_EDGE_COUNT,
    GRAPH_HEADER_FIELD_COUNT
};

}  // namespace wikipath

#endif  // ndef WIKIPATH_GRAPH_HEADER_H_INCLUDED
