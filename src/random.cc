#include <wikipath/random.h>

#include <algorithm>
#include <random>

namespace wikipath {

std::mt19937 CreateRng() {
    unsigned random_data[624];
    std::random_device source;
    std::generate(std::begin(random_data), std::end(random_data), std::ref(source));
    std::seed_seq seed_seq(std::begin(random_data), std::end(random_data));
    return std::mt19937(seed_seq);
}

std::mt19937 &Rng() {
    static std::mt19937 rng = CreateRng();
    return rng;
}

}  // namespace wikipath
