/* Performance note — O(N²) proximity scan
 * ─────────────────────────────────────────
 * The bond tick loop in world.cpp scans all soul pairs every tick.
 * Benchmarks: ~0.65 ms @ 50 souls, ~10 ms @ 200 souls, ~261 ms @ 1000 souls.
 * Fix when needed: spatial grid bucketing reduces to O(N) average case.
 * Target for v0.2: under 5 ms at 1000 souls.
 */

#include "bonds.hpp"
#include <cmath>
#include <algorithm>
#include "../include/samskriti.h"

Bond& bond_get_or_create(BondMap& bonds, int a, int b) {
    std::string key = bond_key(a, b);
    auto it = bonds.find(key);
    if (it != bonds.end()) return it->second;
    Bond& b_ref = bonds[key];
    b_ref.soul_a = std::min(a, b);
    b_ref.soul_b = std::max(a, b);
    return b_ref;
}

/*
 * bond_tick_pair
 *
 * Bond grows from proximity × emotional resonance, NOT from meeting.
 * Two souls 1 unit apart with perfectly opposing rasa states do not bond.
 * Two souls 20 units apart with resonant rasa states bond slowly.
 * This is the emergence, not the rule.
 */
void bond_tick_pair(Bond& bond, float distance,
                    const float* rasa_a, const float* rasa_b, float dt) {
    if (distance > BOND_RANGE) return;

    float proximity = 1.0f - (distance / BOND_RANGE);  /* [0, 1] */

    /* Emotional resonance: cosine similarity of rasa vectors */
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (int i = 0; i < RASA_COUNT; ++i) {
        dot    += rasa_a[i] * rasa_b[i];
        norm_a += rasa_a[i] * rasa_a[i];
        norm_b += rasa_b[i] * rasa_b[i];
    }
    float resonance = 0.0f;
    if (norm_a > 1e-6f && norm_b > 1e-6f)
        resonance = dot / (std::sqrt(norm_a) * std::sqrt(norm_b));
    resonance = std::max(0.0f, resonance);  /* negative resonance = no bond growth */

    /* Novelty factor: diminishing returns on already-strong bonds */
    float novelty = 1.0f / (1.0f + std::log1p(bond.bond_strength));

    float delta_bond = proximity * resonance * novelty * BOND_GROWTH_RATE * dt;
    bond.bond_strength  = std::max(0.0f, bond.bond_strength + delta_bond);
    bond.peak_bond      = std::max(bond.peak_bond, bond.bond_strength);

    /* Trust grows from repeated proximity (familiarity → trust, slowly) */
    bond.familiarity = std::min(1.0f, bond.familiarity + proximity * dt * 0.0002f);
    float trust_delta = bond.familiarity * resonance * TRUST_GROWTH_RATE * dt;
    bond.trust = std::min(1.0f, bond.trust + trust_delta);

    /* Emotional resonance is a rolling lerp toward the current value */
    bond.emotional_resonance += (resonance - bond.emotional_resonance) * 0.01f * dt;

    ++bond.interaction_count;
}

/*
 * bond_crystallise_check
 * After BOND_CRYSTALLIZE_DAYS days above BOND_CRYSTALLIZE_THRESHOLD,
 * a permanent floor is set.  The bond cannot decay below this floor.
 */
void bond_crystallise_check(Bond& bond, int sim_day) {
    (void)sim_day;  /* day counter used externally — here we track via field */
    if (bond.bond_strength >= BOND_CRYSTALLIZE_THRESHOLD) {
        ++bond.days_above_threshold;
        if (bond.days_above_threshold >= BOND_CRYSTALLIZE_DAYS
                && bond.crystallized_floor <= 0.0f) {
            float tier = (bond.bond_strength > 6.0f) ? 0.7f :
                         (bond.bond_strength > 4.0f) ? 0.5f : 0.3f;
            bond.crystallized_floor = bond.bond_strength * tier;
        }
    } else {
        bond.days_above_threshold = 0;
    }
}

void bond_decay(Bond& bond, float dt) {
    float new_bond = std::max(bond.crystallized_floor,
                              bond.bond_strength - BOND_DECAY_RATE * dt);
    bond.bond_strength = new_bond;
}

BondInfo bond_get_info(const Bond& b) {
    BondInfo info = {};
    info.soul_a              = b.soul_a;
    info.soul_b              = b.soul_b;
    info.bond_strength       = b.bond_strength;
    info.trust               = b.trust;
    info.familiarity         = b.familiarity;
    info.emotional_resonance = b.emotional_resonance;
    info.crystallized_floor  = b.crystallized_floor;
    info.peak_bond           = b.peak_bond;
    info.interaction_count   = b.interaction_count;
    return info;
}
