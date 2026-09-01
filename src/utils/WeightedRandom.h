#pragma once
#include <cstdlib>

namespace obfutil {

/// Weighted random selection. Returns an index in [0, count).
/// `weights` is an array of non-negative integers; higher = more likely.
/// Uses rand() — caller is responsible for seeding.
inline int weightedPick(const int *weights, int count) {
    int total = 0;
    for (int i = 0; i < count; i++) total += weights[i];
    int r = rand() % total, cum = 0;
    for (int i = 0; i < count; i++) {
        cum += weights[i];
        if (r < cum) return i;
    }
    return count - 1;
}

} // namespace obfutil
