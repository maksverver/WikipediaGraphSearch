#ifndef WIKIPATH_RANDOM_H_INCLUDED
#define WIKIPATH_RANDOM_H_INCLUDED

#include <random>

namespace wikipath {

std::mt19937 CreateRng();

std::mt19937 &Rng();

template <class T> T RandInt(T min, T max, std::mt19937 &rng = Rng()) {
    return std::uniform_int_distribution<T>(min, max)(rng);
}

}  // wikipath

#endif // ndef WIKIPATH_RANDOM_H_INCLUDED
