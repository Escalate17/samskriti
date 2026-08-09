/**
 * tests/bench_performance.cpp
 *
 * Standalone benchmark — NOT part of the test suite.
 * Measures core operations at multiple soul counts.
 * Reports min/avg/max over 100 runs in microseconds.
 *
 * Build via CMake (bench_performance target) or directly:
 *   clang++ -std=c++17 -O2 -I include \
 *     src/soul.cpp src/bonds.cpp src/world.cpp \
 *     tests/bench_performance.cpp -o build/bench_performance
 */

#include "../include/samskriti.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstring>

using Clock = std::chrono::high_resolution_clock;
using US    = std::chrono::duration<double, std::micro>;

static double now_us() {
    return std::chrono::duration<double, std::micro>(
               Clock::now().time_since_epoch()).count();
}

/* Spawn N souls in a square grid.  spread controls density. */
static void spawn_grid(int n, float spread) {
    SoulConfig cfg = samskriti_default_config();
    int side = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(n))));
    for (int i = 0; i < n; ++i) {
        cfg.position[0] = static_cast<float>(i % side) * spread;
        cfg.position[1] = 0.0f;
        cfg.position[2] = static_cast<float>(i / side) * spread;
        create_soul(cfg);
    }
}

struct Stats { double min_us, avg_us, max_us; };

template<typename Fn>
static Stats measure(int n_runs, Fn fn) {
    std::vector<double> samples;
    samples.reserve(static_cast<size_t>(n_runs));
    for (int r = 0; r < n_runs; ++r) {
        double t0 = now_us();
        fn();
        double t1 = now_us();
        samples.push_back(t1 - t0);
    }
    std::sort(samples.begin(), samples.end());
    double sum = 0;
    for (double v : samples) sum += v;
    return { samples.front(), sum / samples.size(), samples.back() };
}

/* ── Table formatting ─────────────────────────────────────────── */

static void print_header() {
    printf("\n");
    printf("┌──────────────────────────┬────────────┬────────────┬────────────┐\n");
    printf("│  Operation               │  Min (µs)  │  Avg (µs)  │  Max (µs)  │\n");
    printf("├──────────────────────────┼────────────┼────────────┼────────────┤\n");
}

static void print_row(const char* label, Stats s) {
    printf("│  %-24s│  %8.1f  │  %8.1f  │  %8.1f  │\n",
           label, s.min_us, s.avg_us, s.max_us);
}

static void print_divider() {
    printf("├──────────────────────────┼────────────┼────────────┼────────────┤\n");
}

static void print_footer() {
    printf("└──────────────────────────┴────────────┴────────────┴────────────┘\n");
}

/* ═══════════════════════════════════════════════════════════════ */

int main() {
    const int RUNS         = 100;
    const int WARMUP_TICKS = 20;  /* let bonds form before timing */
    const float SPREAD     = 10.0f; /* medium density */

    printf("Samskriti Core — Performance Benchmark\n");
    printf("Runs per measurement: %d\n", RUNS);
    printf("Soul spread: %.1f units (medium density)\n\n", SPREAD);

    print_header();

    /* ── tick() at increasing soul counts ─────────────────────── */
    for (int n : {10, 50, 100, 200, 500, 1000}) {
        samskriti_init(0xBEEF0000ULL + static_cast<uint64_t>(n));
        spawn_grid(n, SPREAD);
        for (int i = 0; i < WARMUP_TICKS; ++i) tick(1.0f);

        char label[32];
        std::snprintf(label, sizeof(label), "tick()  %4d souls", n);
        Stats s = measure(RUNS, []{ tick(1.0f); });
        print_row(label, s);

        if (n == 500) print_divider();  /* visual break before 1000 */
    }

    print_divider();

    /* ── inject_event() at 100 souls ──────────────────────────── */
    {
        samskriti_init(0xC0FFEE11ULL);
        spawn_grid(100, SPREAD);
        for (int i = 0; i < WARMUP_TICKS; ++i) tick(1.0f);

        int victim = 0, attacker = 1;
        Stats s = measure(RUNS, [&]{
            inject_event(victim, "attacked", 0.5f, attacker);
        });
        print_row("inject_event()  100s", s);
    }

    /* ── get_state() at 100 souls ─────────────────────────────── */
    {
        samskriti_init(0xC0FFEE22ULL);
        spawn_grid(100, SPREAD);
        for (int i = 0; i < WARMUP_TICKS; ++i) tick(1.0f);

        Stats s = measure(RUNS, []{ get_state(0); });
        print_row("get_state()     100s", s);
    }

    /* ── save + load round-trip at 100 souls ──────────────────── */
    {
        samskriti_init(0xC0FFEE33ULL);
        spawn_grid(100, SPREAD);
        for (int i = 0; i < WARMUP_TICKS; ++i) tick(1.0f);

        const char* path = "/tmp/samskriti_bench.bin";
        Stats s = measure(RUNS, [&]{
            save_world(path);
            load_world(path);
        });
        print_row("save+load       100s", s);
    }

    print_footer();

    /* ── Dense packing note ───────────────────────────────────── */
    printf("\nDense packing comparison (spread=5.0, all souls within BOND_RANGE):\n");
    print_header();
    for (int n : {50, 200, 1000}) {
        samskriti_init(0xDA000000ULL + static_cast<uint64_t>(n));
        spawn_grid(n, 5.0f);
        for (int i = 0; i < WARMUP_TICKS; ++i) tick(1.0f);

        char label[32];
        std::snprintf(label, sizeof(label), "tick()  %4d souls  dense", n);
        Stats s = measure(RUNS, []{ tick(1.0f); });
        print_row(label, s);
    }
    print_footer();
    printf("Note: dense packing puts all souls in 1-4 grid cells.\n");
    printf("Spatial bucketing has limited benefit — all pairs are in-range.\n");
    printf("Real-world distributions (10+ units spacing) hit the faster path.\n\n");

    return 0;
}
