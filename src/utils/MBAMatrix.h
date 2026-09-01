#pragma once
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace obfutil {

/// NxN matrix of uint64_t (mod 2^64 arithmetic).
using Matrix = std::vector<std::vector<uint64_t>>;

/// Build a random invertible matrix A and its inverse A_inv simultaneously.
/// Uses elementary row operations so no Gaussian elimination is needed.
/// Both are DIM×DIM identity initially, then scrambled with `steps` random
/// row additions using odd multipliers (invertible mod 2^64).
inline void createMatrixPair(Matrix &A, Matrix &A_inv,
                              int dim = 5, int steps = 20) {
    A.assign(dim, std::vector<uint64_t>(dim, 0));
    A_inv.assign(dim, std::vector<uint64_t>(dim, 0));
    for (int i = 0; i < dim; ++i) {
        A[i][i] = 1;
        A_inv[i][i] = 1;
    }
    for (int step = 0; step < steps; ++step) {
        int r1 = rand() % dim, r2 = rand() % dim;
        if (r1 == r2) continue;
        uint64_t k = (uint64_t)rand() | 1ULL;
        for (int c = 0; c < dim; ++c)
            A[r1][c] += k * A[r2][c];
        for (int r = 0; r < dim; ++r)
            A_inv[r][r2] -= k * A_inv[r][r1];
    }
}

/// Compute C = A_inv * T  (matrix–vector product).
inline std::vector<uint64_t> computeConstants(const Matrix &A_inv,
                                               const std::vector<uint64_t> &T) {
    int dim = (int)T.size();
    std::vector<uint64_t> C(dim, 0);
    for (int r = 0; r < dim; ++r)
        for (int c = 0; c < dim; ++c)
            C[r] += A_inv[r][c] * T[c];
    return C;
}

} // namespace obfutil
