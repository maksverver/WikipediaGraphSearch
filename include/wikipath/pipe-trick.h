#ifndef WIKIPATH_PIPETRICK_H_INCLUDED
#define WIKIPATH_PIPETRICK_H_INCLUDED

#include <string_view>

namespace wikipath {

// Simplifies the string according to the Wikipedia pipe trick rules:
// https://en.wikipedia.org/wiki/Help:Pipe_trick
//
// Not all corner cases are handled because Wikipedia doesn't fully specify
// them. See pipe-trick_test.cc for some examples that are handled correctly.
std::string_view ResolvePipeTrick(std::string_view s);

}  // namespace wikipath

#endif  // ndef WIKIPATH_PIPETRICK_H_INCLUDED
