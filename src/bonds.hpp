#pragma once
#include <unordered_map>
#include <string>
#include "../include/samskriti.h"

static constexpr float BOND_CRYSTALLIZE_THRESHOLD = 3.0f;
static constexpr int   BOND_CRYSTALLIZE_DAYS       = 20;
static constexpr float BOND_RANGE                  = 30.0f;
static constexpr float BOND_GROWTH_RATE            = 0.002f;
static constexpr float TRUST_GROWTH_RATE           = 0.001f;
static constexpr float BOND_DECAY_RATE             = 0.001f;

struct Bond {
    int   soul_a = -1;
    int   soul_b = -1;
    float bond_strength       = 0.0f;
    float trust               = 0.0f;
    float familiarity         = 0.0f;
    float emotional_resonance = 0.0f;
    float crystallized_floor  = 0.0f;
    float peak_bond           = 0.0f;
    int   interaction_count   = 0;
    int   days_above_threshold = 0;
};

/* Key is always "min_id:max_id" */
inline std::string bond_key(int a, int b) {
    if (a > b) { int t = a; a = b; b = t; }
    return std::to_string(a) + ':' + std::to_string(b);
}

using BondMap = std::unordered_map<std::string, Bond>;

/* Get or create; does NOT grow the bond */
Bond& bond_get_or_create(BondMap& bonds, int a, int b);

/* Tick proximity-based bond formation for one pair. */
void bond_tick_pair(Bond& bond, float distance,
                    const float* rasa_a, const float* rasa_b, float dt);

/* Crystallisation check — call once per sim day. */
void bond_crystallise_check(Bond& bond, int sim_day);

/* Decay sweep — call every tick. */
void bond_decay(Bond& bond, float dt);

/* BondInfo public snapshot */
BondInfo bond_get_info(const Bond& b);
