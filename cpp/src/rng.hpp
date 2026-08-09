#pragma once
#include <cstdint>

/**
 * Splitmix64 RNG — single 64-bit state, trivially serialisable.
 * All stochastic draws in the core go through here.  No std::rand().
 * This is what makes tick() deterministic: same state → same sequence.
 */
struct WorldRng {
    uint64_t state = 0x9e3779b97f4a7c15ULL;

    void reseed(uint64_t s) {
        state = (s != 0) ? s : 0x9e3779b97f4a7c15ULL;
    }

    uint64_t next() {
        state += 0x9e3779b97f4a7c15ULL;
        uint64_t z = state;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    /* [0, 1) */
    float uniform() {
        return static_cast<float>(next() >> 11) * (1.0f / static_cast<float>(1ULL << 53));
    }

    float range(float lo, float hi) { return lo + uniform() * (hi - lo); }

    int randi(int lo, int hi) {
        if (lo >= hi) return lo;
        return lo + static_cast<int>(next() % static_cast<uint64_t>(hi - lo + 1));
    }

    /* Serialise / restore — the full state is just one uint64. */
    uint64_t snapshot() const { return state; }
    void     restore(uint64_t s) { state = s; }
};

extern WorldRng g_rng;
