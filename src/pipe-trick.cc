#include "wikipath/pipe-trick.h"

#include <cctype>
#include <string_view>

namespace wikipath {

// Note: this function doesn't try to handle corner cases correctly because
// Wikipedia doesn't fully define them
std::string_view ResolvePipeTrick(std::string_view s) {
    // Remove prefix up to and including the first colon, plus the leading colon, if present.
    if (!s.empty()) {
        std::string_view::size_type colon_pos = s.find(':', 1);
        if (colon_pos != std::string_view::npos) s.remove_prefix(colon_pos);
        if (!s.empty() && s[0] == ':') s.remove_prefix(1);
    }

    // Remove suffix starting with the last opening parenthesis.
    std::string_view::size_type lparen_pos = s.rfind('(');
    if (lparen_pos != std::string_view::npos) {
        s = s.substr(0, lparen_pos);
    } else {
        // Otherwise, remove suffix starting from the first comma.
        std::string_view::size_type comma_pos = s.find(',');
        if (comma_pos != std::string_view::npos) {
            s = s.substr(0, comma_pos);
        }
    }

    // Remove leading and trailing whitespace.
    while (!s.empty() && isspace(*s.begin())) s.remove_prefix(1);
    while (!s.empty() && isspace(*s.rbegin())) s.remove_suffix(1);

    return s;
}

}  // namespace wikipath
