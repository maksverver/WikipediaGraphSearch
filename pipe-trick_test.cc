#include "pipe-trick.h"

#include <iostream>
#include <string_view>

#include <stdlib.h>

struct TestCase {
    std::string_view input;
    std::string_view output;
};

// Examples from:
// https://en.wikipedia.org/wiki/Help:Pipe_trick
constexpr const TestCase test_cases[] = {
    {"", ""},
    {"Foo Bar", "Foo Bar"},
    {"Pipe (computing)", "Pipe"},
    {"Phoenix, Arizona", "Phoenix"},
    {"Wikipedia:Verifiability", "Verifiability"},
    {"Yours, Mine and Ours (1968 film)", "Yours, Mine and Ours"},
    {":es:Wikipedia:Políticas", "Wikipedia:Políticas"},
    {"Il Buono, il Brutto, il Cattivo", "Il Buono"},
    {"Wikipedia:Manual of Style (Persian)", "Manual of Style"},
    {":Test", "Test"},
    {"\t Whitespace \n", "Whitespace"},
    {"Test (foo) (bar) (baz)", "Test (foo) (bar)"},
};

int main() {
    int successes = 0, failures = 0;
    for (auto [input, expected_output] : test_cases) {
        std::string_view received_output = ResolvePipeTrick(input);
        if (received_output == expected_output) {
            ++successes;
        } else {
            ++failures;
            std::cout << "Test failed!\n"
                << "\tInput: [" << input << "]\n"
                << "\tExpected output: [" << expected_output << "]\n"
                << "\tReceived output: [" << received_output << "]\n";
        }
    }
    if (failures > 0) {
        std::cout << failures << " tests failed!\n";
        return EXIT_FAILURE;
    } else {
        std::cout << "All " << successes << " tests passed.\n";
        return EXIT_SUCCESS;
    }
}
