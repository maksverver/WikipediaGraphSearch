#ifndef WIKIPATH_COMMON_H_INCLUDED
#define WIKIPATH_COMMON_H_INCLUDED

#include <stdint.h>

namespace wikipath {

// The type used for page indices. 32 bits ought to be enough for anyone.
// Guaranteed to be an unsigned type.
typedef uint32_t index_t;

}  // namespace wikipath

#endif  // ndef WIKIPATH_COMMON_H_INCLUDED
