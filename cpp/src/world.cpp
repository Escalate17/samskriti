#include "world.hpp"
#include "../include/samskriti.h"
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <string>

/* ── Global singletons ───────────────────────────────────────── */
WorldRng g_rng;
World    g_world;

/* ── FNV-1a 64-bit hash ─────────────────────────────────────── */
static uint64_t fnv1a_update(uint64_t h, const void* data, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<uint64_t>(p[i]);
        h *= 0x00000100000001B3ULL;
    }
    return h;
}

/* ── World helpers ───────────────────────────────────────────── */
Soul* World::find_soul(int id) {
    auto it = id_to_index.find(id);
    if (it == id_to_index.end()) return nullptr;
    int idx = it->second;
    if (idx < 0 || idx >= static_cast<int>(souls.size())) return nullptr;
    return &souls[static_cast<size_t>(idx)];
}

Bond* World::find_bond(int a, int b) {
    std::string key = bond_key(a, b);
    auto it = bonds.find(key);
    if (it == bonds.end()) return nullptr;
    return &it->second;
}

void World::clear() {
    souls.clear();
    bonds.clear();
    id_to_index.clear();
    next_soul_id = 0;
    sim_tick     = 0;
    sim_day      = 0;
}

/* ═══════════════════════════════════════════════════════════════
 * World::create_soul
 * ═══════════════════════════════════════════════════════════════ */
int World::create_soul(const SoulConfig& cfg) {
    int id = next_soul_id++;
    souls.emplace_back();
    Soul& s = souls.back();
    soul_init(s, cfg, id);

    /* Lineage inheritance — if parents exist, pull their rasa echo */
    auto inherit_from = [&](int parent_id) {
        Soul* p = find_soul(parent_id);
        if (!p) return;
        for (int i = 0; i < RASA_COUNT; ++i)
            s.inherited_rasa[i] = std::max(s.inherited_rasa[i], p->rasa[i] * 0.3f);
        /* karma_rina inherits with a small mutation */
        float kr = (s.karma_rina + p->karma_rina) * 0.5f + g_rng.range(-0.05f, 0.05f);
        s.karma_rina = kr < -1.0f ? -1.0f : (kr > 1.0f ? 1.0f : kr);
        s.generation = std::max(s.generation, p->generation + 1);
    };
    inherit_from(cfg.parent_a_id);
    inherit_from(cfg.parent_b_id);

    id_to_index[id] = static_cast<int>(souls.size()) - 1;
    return id;
}

void World::destroy_soul(int id) {
    Soul* s = find_soul(id);
    if (s) s->is_alive = false;
}

void World::set_position(int id, float x, float y, float z) {
    Soul* s = find_soul(id);
    if (!s) return;
    s->position[0] = x; s->position[1] = y; s->position[2] = z;
}

/* ═══════════════════════════════════════════════════════════════
 * World::inject_event
 * ═══════════════════════════════════════════════════════════════ */
void World::inject_event(int id, const char* evt, float intensity, int src) {
    Soul* s = find_soul(id);
    if (!s || !s->is_alive) return;

    float bond_str = 0.0f, trust = 0.0f, dvesha = 0.0f;
    if (src >= 0) {
        Bond* b = find_bond(id, src);
        if (b) {
            bond_str = b->bond_strength;
            trust    = b->trust;
        }
        /* dvesha is stored in the soul's rasa — it's the per-target aversion
         * crystallised over time.  We use the soul's current dvesha level as
         * a proxy (a more complete impl would index by source_id). */
        dvesha = s->rasa[RASA_DVESHA];
    }

    soul_inject_event(*s, evt, intensity, src, bond_str, trust, dvesha);
}

/* ═══════════════════════════════════════════════════════════════
 * World::tick
 * ═══════════════════════════════════════════════════════════════ */
void World::tick(float dt) {
    /* ── Build spatial grid ──────────────────────────────────────
     * Cell size = BOND_RANGE (30.0).  Rebuilt each tick (souls move).
     * Step 1 bond scan: O(N) average.
     * Step 3 nearest search: expanding Chebyshev ring, stops early.
     * ──────────────────────────────────────────────────────────── */
    static constexpr float GRID_CELL = BOND_RANGE;

    /* Pack (gx, gz) as int64_t — handles negative grid coords */
    auto cell_key = [](int gx, int gz) noexcept -> int64_t {
        return (static_cast<int64_t>(static_cast<uint32_t>(gx)) << 32)
             | static_cast<int64_t>(static_cast<uint32_t>(gz));
    };

    std::unordered_map<int64_t, std::vector<size_t>> grid;
    grid.reserve(souls.size() * 2);
    for (size_t i = 0; i < souls.size(); ++i) {
        if (!souls[i].is_alive) continue;
        int gx = static_cast<int>(std::floor(souls[i].position[0] / GRID_CELL));
        int gz = static_cast<int>(std::floor(souls[i].position[2] / GRID_CELL));
        grid[cell_key(gx, gz)].push_back(i);
    }

    /* ── Step 1: perception signals + bond tick ─────────────────
     * Each soul scans its 3×3 cell neighbourhood.
     * Pair (i,j) processed exactly once: only when j > i.         */
    for (Soul& s : souls) {
        if (!s.is_alive) continue;
        s.isolation        = 1.0f;
        s.bonded_presence  = 0.0f;
        s.void_proximity   = 0.0f;
        s.world_certainty  = 1.0f;
    }

    for (size_t i = 0; i < souls.size(); ++i) {
        Soul& sa = souls[i];
        if (!sa.is_alive) continue;
        int gx = static_cast<int>(std::floor(sa.position[0] / GRID_CELL));
        int gz = static_cast<int>(std::floor(sa.position[2] / GRID_CELL));

        for (int cx = -1; cx <= 1; ++cx) {
            for (int cz = -1; cz <= 1; ++cz) {
                auto it = grid.find(cell_key(gx + cx, gz + cz));
                if (it == grid.end()) continue;
                for (size_t j : it->second) {
                    if (j <= i) continue;   /* each pair exactly once */
                    Soul& sb = souls[j];
                    if (!sb.is_alive) continue;

                    float ddx = sa.position[0] - sb.position[0];
                    float ddy = sa.position[1] - sb.position[1];
                    float ddz = sa.position[2] - sb.position[2];
                    float dist = std::sqrt(ddx*ddx + ddy*ddy + ddz*ddz);

                    if (dist > BOND_RANGE * 2.0f) continue;

                    float prox = 1.0f - dist / (BOND_RANGE * 2.0f);

                    sa.isolation = std::min(sa.isolation, 1.0f - prox * 0.7f);
                    sb.isolation = std::min(sb.isolation, 1.0f - prox * 0.7f);

                    Bond* b = find_bond(sa.id, sb.id);
                    if (b && b->bond_strength >= 1.0f) {
                        sa.bonded_presence = std::min(1.0f, sa.bonded_presence + prox * 0.5f);
                        sb.bonded_presence = std::min(1.0f, sb.bonded_presence + prox * 0.5f);
                    }

                    Bond& bond = bond_get_or_create(bonds, sa.id, sb.id);
                    bond_tick_pair(bond, dist, sa.rasa, sb.rasa, dt);
                    bond_decay(bond, dt);
                }
            }
        }
    }

    /* ── Step 2: Crystallisation check (once per day) ─────────── */
    int new_day = sim_tick / TICKS_PER_DAY;
    if (new_day > sim_day) {
        sim_day = new_day;
        for (auto& kv : bonds)
            bond_crystallise_check(kv.second, sim_day);
    }

    /* ── Step 3: nearest bonded/any for buddhi ──────────────────
     * Same 3×3 neighbourhood as Step 1.  Bonds only form within
     * BOND_RANGE (30 units), so any crystallised bond partner is
     * always reachable in this neighbourhood. */
    for (Soul& s : souls) {
        if (!s.is_alive) continue;
        float nearest_bonded = 1e9f, nearest_any = 1e9f;
        int   nearest_bonded_id = -1;

        int gx = static_cast<int>(std::floor(s.position[0] / GRID_CELL));
        int gz = static_cast<int>(std::floor(s.position[2] / GRID_CELL));

        for (int cx = -1; cx <= 1; ++cx) {
            for (int cz = -1; cz <= 1; ++cz) {
                auto it = grid.find(cell_key(gx + cx, gz + cz));
                if (it == grid.end()) continue;
                for (size_t j : it->second) {
                    const Soul& other = souls[j];
                    if (!other.is_alive || other.id == s.id) continue;
                    float dx = s.position[0] - other.position[0];
                    float dy = s.position[1] - other.position[1];
                    float dz = s.position[2] - other.position[2];
                    float d  = std::sqrt(dx*dx + dy*dy + dz*dz);
                    if (d < nearest_any) nearest_any = d;
                    Bond* b = find_bond(s.id, other.id);
                    if (b && b->bond_strength >= 1.0f && d < nearest_bonded) {
                        nearest_bonded    = d;
                        nearest_bonded_id = other.id;
                    }
                }
            }
        }

        soul_update_buddhi(s, nearest_bonded, nearest_any, nearest_bonded_id);
    }

    /* ── Step 4: Tick each soul ─────────────────────────────────── */
    for (Soul& s : souls) {
        soul_tick(s, dt);
    }

    ++sim_tick;
}

/* ═══════════════════════════════════════════════════════════════
 * Binary save / load  (little-endian, portable within the lib)
 *
 * Format:
 *   [8]  magic "SMSKRTI\0"
 *   [4]  version = 1
 *   [8]  rng_state
 *   [4]  sim_tick
 *   [4]  sim_day
 *   [4]  next_soul_id
 *   [4]  n_souls
 *   n_souls × soul_record
 *   [4]  n_bonds
 *   n_bonds × bond_record
 * ═══════════════════════════════════════════════════════════════ */

static void write_u8 (FILE* f, const void* d, size_t n) { fwrite(d, 1, n, f); }
static void write_u32(FILE* f, uint32_t v)  { fwrite(&v, 4, 1, f); }
static void write_u64(FILE* f, uint64_t v)  { fwrite(&v, 8, 1, f); }
static void write_f32(FILE* f, float v)     { fwrite(&v, 4, 1, f); }
static void write_i32(FILE* f, int32_t v)   { fwrite(&v, 4, 1, f); }

static bool read_u32(FILE* f, uint32_t& v)  { return fread(&v, 4, 1, f) == 1; }
static bool read_u64(FILE* f, uint64_t& v)  { return fread(&v, 8, 1, f) == 1; }
static bool read_f32(FILE* f, float& v)     { return fread(&v, 4, 1, f) == 1; }
static bool read_i32(FILE* f, int32_t& v)   { return fread(&v, 4, 1, f) == 1; }

static void save_soul(FILE* f, const Soul& s) {
    write_i32(f, s.id);
    write_u32(f, s.is_alive ? 1u : 0u);
    write_u8(f, s.position, sizeof(s.position));
    write_u8(f, s.rasa,     sizeof(s.rasa));
    write_u8(f, s.gunas,    sizeof(s.gunas));
    write_u8(f, s.vasanas,  sizeof(s.vasanas));
    write_i32(f, s.samskara_head);
    write_i32(f, s.samskara_count);
    for (int i = 0; i < MAX_SAMSKARAS; ++i) {
        const Samskara& sm = s.samskara_ring[i];
        write_u8(f, sm.rasa_at_moment, sizeof(sm.rasa_at_moment));
        write_i32(f, sm.dominant_rasa);
        write_f32(f, sm.valence);
        write_f32(f, sm.strength);
        write_i32(f, sm.buddhi_at_moment);
        write_i32(f, sm.context_hash);
        write_i32(f, sm.source_soul_id);
    }
    write_f32(f, s.identity_coherence);
    write_f32(f, s.prana);
    write_f32(f, s.health);
    write_i32(f, s.age);
    write_i32(f, s.generation);
    write_f32(f, s.karma_rina);
    write_i32(f, s.parent_a_id);
    write_i32(f, s.parent_b_id);
    write_u8(f, s.inherited_rasa, sizeof(s.inherited_rasa));
    write_i32(f, static_cast<int32_t>(s.buddhi_action));
    write_i32(f, s.buddhi_target_id);
    write_f32(f, s.buddhi_timer);
    write_f32(f, s.shoka_sustained);
    write_i32(f, s.last_event_source_id);
    write_f32(f, s.disposition_toward_source);
    write_f32(f, s.isolation);
    write_f32(f, s.bonded_presence);
    write_f32(f, s.void_proximity);
    write_f32(f, s.world_certainty);
    write_u8(f, s.prev_rasa, sizeof(s.prev_rasa));
    write_f32(f, s.rasa_event_timer);
}

static bool load_soul(FILE* f, Soul& s) {
    int32_t id; if (!read_i32(f, id)) return false; s.id = id;
    uint32_t alive; if (!read_u32(f, alive)) return false; s.is_alive = (alive != 0);
    if (fread(s.position, sizeof(s.position), 1, f) != 1) return false;
    if (fread(s.rasa,     sizeof(s.rasa),     1, f) != 1) return false;
    if (fread(s.gunas,    sizeof(s.gunas),    1, f) != 1) return false;
    if (fread(s.vasanas,  sizeof(s.vasanas),  1, f) != 1) return false;
    int32_t sh, sc;
    if (!read_i32(f, sh) || !read_i32(f, sc)) return false;
    s.samskara_head  = sh;
    s.samskara_count = sc;
    for (int i = 0; i < MAX_SAMSKARAS; ++i) {
        Samskara& sm = s.samskara_ring[i];
        if (fread(sm.rasa_at_moment, sizeof(sm.rasa_at_moment), 1, f) != 1) return false;
        int32_t dr; if (!read_i32(f, dr)) return false; sm.dominant_rasa = dr;
        if (!read_f32(f, sm.valence))  return false;
        if (!read_f32(f, sm.strength)) return false;
        int32_t bam, ch, ssi;
        if (!read_i32(f, bam) || !read_i32(f, ch) || !read_i32(f, ssi)) return false;
        sm.buddhi_at_moment = bam;
        sm.context_hash     = ch;
        sm.source_soul_id   = ssi;
    }
    if (!read_f32(f, s.identity_coherence)) return false;
    if (!read_f32(f, s.prana))   return false;
    if (!read_f32(f, s.health))  return false;
    int32_t age, gen;
    if (!read_i32(f, age) || !read_i32(f, gen)) return false;
    s.age = age; s.generation = gen;
    if (!read_f32(f, s.karma_rina)) return false;
    int32_t pa, pb;
    if (!read_i32(f, pa) || !read_i32(f, pb)) return false;
    s.parent_a_id = pa; s.parent_b_id = pb;
    if (fread(s.inherited_rasa, sizeof(s.inherited_rasa), 1, f) != 1) return false;
    int32_t ba;
    if (!read_i32(f, ba)) return false; s.buddhi_action = static_cast<BuddhiAction>(ba);
    int32_t bti;
    if (!read_i32(f, bti)) return false; s.buddhi_target_id = bti;
    if (!read_f32(f, s.buddhi_timer))    return false;
    if (!read_f32(f, s.shoka_sustained)) return false;
    int32_t lesi;
    if (!read_i32(f, lesi)) return false; s.last_event_source_id = lesi;
    if (!read_f32(f, s.disposition_toward_source)) return false;
    if (!read_f32(f, s.isolation))       return false;
    if (!read_f32(f, s.bonded_presence)) return false;
    if (!read_f32(f, s.void_proximity))  return false;
    if (!read_f32(f, s.world_certainty)) return false;
    if (fread(s.prev_rasa, sizeof(s.prev_rasa), 1, f) != 1) return false;
    if (!read_f32(f, s.rasa_event_timer)) return false;
    return true;
}

void World::save(const char* path) const {
    FILE* f = fopen(path, "wb");
    if (!f) return;

    /* Magic + version */
    fwrite("SMSKRTI\0", 8, 1, f);
    write_u32(f, 1u);  /* version */

    write_u64(f, g_rng.snapshot());
    write_i32(f, sim_tick);
    write_i32(f, sim_day);
    write_i32(f, next_soul_id);

    write_u32(f, static_cast<uint32_t>(souls.size()));
    for (const Soul& s : souls) save_soul(f, s);

    /* Bonds: write sorted by key for canonical ordering */
    std::vector<std::string> keys;
    keys.reserve(bonds.size());
    for (const auto& kv : bonds) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());

    write_u32(f, static_cast<uint32_t>(keys.size()));
    for (const std::string& key : keys) {
        const Bond& b = bonds.at(key);
        write_i32(f, b.soul_a);
        write_i32(f, b.soul_b);
        write_f32(f, b.bond_strength);
        write_f32(f, b.trust);
        write_f32(f, b.familiarity);
        write_f32(f, b.emotional_resonance);
        write_f32(f, b.crystallized_floor);
        write_f32(f, b.peak_bond);
        write_i32(f, b.interaction_count);
        write_i32(f, b.days_above_threshold);
    }

    fclose(f);
}

void World::load(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return;

    char magic[8];
    if (fread(magic, 8, 1, f) != 1
        || memcmp(magic, "SMSKRTI\0", 8) != 0) { fclose(f); return; }

    uint32_t version; read_u32(f, version);
    if (version != 1u) { fclose(f); return; }

    clear();

    uint64_t rng_state; read_u64(f, rng_state);
    g_rng.restore(rng_state);

    int32_t st, sd, nsi;
    read_i32(f, st); read_i32(f, sd); read_i32(f, nsi);
    sim_tick     = st;
    sim_day      = sd;
    next_soul_id = nsi;

    uint32_t n_souls; read_u32(f, n_souls);
    souls.resize(n_souls);
    for (uint32_t i = 0; i < n_souls; ++i) {
        load_soul(f, souls[i]);
        id_to_index[souls[i].id] = static_cast<int>(i);
    }

    uint32_t n_bonds; read_u32(f, n_bonds);
    for (uint32_t i = 0; i < n_bonds; ++i) {
        Bond b = {};
        int32_t ic, dat;
        read_i32(f, b.soul_a);
        read_i32(f, b.soul_b);
        read_f32(f, b.bond_strength);
        read_f32(f, b.trust);
        read_f32(f, b.familiarity);
        read_f32(f, b.emotional_resonance);
        read_f32(f, b.crystallized_floor);
        read_f32(f, b.peak_bond);
        read_i32(f, ic);  b.interaction_count    = ic;
        read_i32(f, dat); b.days_above_threshold = dat;
        bonds[bond_key(b.soul_a, b.soul_b)] = b;
    }

    fclose(f);
}

/* ═══════════════════════════════════════════════════════════════
 * World::hash
 * FNV-1a over the canonical world state.
 * Souls sorted by id, bonds sorted by key — deterministic order.
 * ═══════════════════════════════════════════════════════════════ */
uint64_t World::hash() const {
    static constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
    uint64_t h = FNV_OFFSET;

    /* RNG state */
    uint64_t rng = g_rng.snapshot();
    h = fnv1a_update(h, &rng, 8);

    /* World counters */
    h = fnv1a_update(h, &sim_tick,     4);
    h = fnv1a_update(h, &sim_day,      4);
    h = fnv1a_update(h, &next_soul_id, 4);

    /* Souls sorted by id */
    std::vector<int> ids;
    ids.reserve(souls.size());
    for (const Soul& s : souls) ids.push_back(s.id);
    std::sort(ids.begin(), ids.end());

    for (int id : ids) {
        const Soul* sp = const_cast<World*>(this)->find_soul(id);
        if (!sp) continue;
        const Soul& s = *sp;
        h = fnv1a_update(h, &s.id,       4);
        h = fnv1a_update(h, &s.is_alive, 1);
        h = fnv1a_update(h, s.rasa,      sizeof(s.rasa));
        h = fnv1a_update(h, s.gunas,     sizeof(s.gunas));
        h = fnv1a_update(h, s.vasanas,   sizeof(s.vasanas));
        h = fnv1a_update(h, &s.samskara_count, 4);
        for (int i = 0; i < s.samskara_count; ++i) {
            int ri = (s.samskara_head - s.samskara_count + i + MAX_SAMSKARAS) % MAX_SAMSKARAS;
            const Samskara& sm = s.samskara_ring[ri];
            h = fnv1a_update(h, sm.rasa_at_moment, sizeof(sm.rasa_at_moment));
            h = fnv1a_update(h, &sm.valence,   4);
            h = fnv1a_update(h, &sm.strength,  4);
        }
        h = fnv1a_update(h, &s.identity_coherence, 4);
        h = fnv1a_update(h, &s.age,        4);
        h = fnv1a_update(h, &s.generation, 4);
        h = fnv1a_update(h, &s.karma_rina, 4);
    }

    /* Bonds sorted by key */
    std::vector<std::string> keys;
    keys.reserve(bonds.size());
    for (const auto& kv : bonds) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());

    for (const std::string& key : keys) {
        const Bond& b = bonds.at(key);
        h = fnv1a_update(h, &b.soul_a,        4);
        h = fnv1a_update(h, &b.soul_b,        4);
        h = fnv1a_update(h, &b.bond_strength, 4);
        h = fnv1a_update(h, &b.trust,         4);
        h = fnv1a_update(h, &b.crystallized_floor, 4);
    }

    return h;
}

/* ═══════════════════════════════════════════════════════════════
 * Behavior hint computation (stateless read — no world mutation)
 * ═══════════════════════════════════════════════════════════════ */
static BehaviorHint compute_behavior_hint(const Soul& s, const Bond* bond) {
    BehaviorHint hint = {};

    float bhayanaka = s.rasa[RASA_BHAYANAKA];
    float krodha    = s.rasa[RASA_KRODHA];
    float shoka     = s.rasa[RASA_SHOKA];
    float shringara = s.rasa[RASA_SHRINGARA];
    float karuna    = s.rasa[RASA_KARUNA];
    float shanta    = s.rasa[RASA_SHANTA];
    float dvesha    = s.rasa[RASA_DVESHA];
    float vishada   = s.rasa[RASA_VISHADA];

    float bond_str = bond ? bond->bond_strength    : 0.0f;
    float trust    = bond ? bond->trust            : 0.0f;
    float cryst    = bond ? bond->crystallized_floor : 0.0f;

    float scores[8] = {};

    /* APPROACH_WARM: love/compassion + existing bond */
    scores[BEHAVIOR_APPROACH_WARM]    = shringara * 0.6f + karuna * 0.4f
                                       + (bond_str > 0.0f ? std::min(bond_str / 4.0f, 0.4f) : 0.0f)
                                       + trust * 0.3f - dvesha * 0.6f - bhayanaka * 0.3f;
    /* APPROACH_NEUTRAL: equanimity-driven curiosity */
    scores[BEHAVIOR_APPROACH_NEUTRAL] = shanta * 0.35f + karuna * 0.2f
                                       + s.rasa[RASA_ADBHUTA] * 0.2f
                                       - bhayanaka * 0.35f - dvesha * 0.4f;
    /* RETREAT_WARY: low-level fear, some existing relationship */
    scores[BEHAVIOR_RETREAT_WARY]     = bhayanaka * 0.5f + s.rasa[RASA_UDVEGA] * 0.3f
                                       - trust * 0.35f - shringara * 0.2f;
    /* RETREAT_FEARFUL: strong fear + mistrust */
    scores[BEHAVIOR_RETREAT_FEARFUL]  = bhayanaka * 0.75f + dvesha * 0.2f
                                       - trust * 0.5f - bond_str * 0.1f;
    /* IGNORE: no strong pull or push */
    scores[BEHAVIOR_IGNORE]           = shanta * 0.25f - shringara * 0.3f
                                       - krodha * 0.25f - bhayanaka * 0.25f
                                       + 0.05f; /* small baseline */
    /* AGGRESS: wrath + crystallised aversion */
    scores[BEHAVIOR_AGGRESS]          = krodha * 0.7f + dvesha * 0.65f
                                       - trust * 0.45f - shanta * 0.3f;
    /* MOURN: grief/shoka dominant */
    scores[BEHAVIOR_MOURN]            = shoka * 0.85f + vishada * 0.4f
                                       - s.rasa[RASA_HASYA] * 0.3f;
    /* REST: low prana, vishada, tamas */
    scores[BEHAVIOR_REST]             = (1.0f - s.prana) * 0.5f + vishada * 0.4f
                                       + s.gunas[GUNA_TAMAS] * 0.3f - shringara * 0.2f;

    /* Winner */
    int best = 0;
    for (int i = 1; i < 8; ++i)
        if (scores[i] > scores[best]) best = i;

    hint.action    = static_cast<BehaviorAction>(best);
    float raw_int  = scores[best];
    hint.intensity = raw_int < 0.0f ? 0.0f : (raw_int > 1.0f ? 1.0f : raw_int);

    /* Tone — net-score approach so bond/trust can override global hostility.
     * trust*2 + bond_str*0.5 lets a known bonded partner feel safer even when
     * the soul carries general fear/anger from a third-party event. */
    float net = (shringara + karuna) * 0.2f
              + trust * 2.0f + bond_str * 0.5f + cryst * 0.5f
              - (dvesha + krodha) * 1.5f - bhayanaka * 0.5f;
    if      (net > 0.25f && cryst > 0.0f)  hint.tone = TONE_FAMILIAR;
    else if (net > 0.08f)                   hint.tone = TONE_WARM;
    else if (net > -0.04f)                  hint.tone = TONE_NEUTRAL;
    else if (net > -0.20f)                  hint.tone = TONE_COLD;
    else                                    hint.tone = TONE_HOSTILE;

    return hint;
}

/* ═══════════════════════════════════════════════════════════════
 * Dialogue modifier computation
 * ═══════════════════════════════════════════════════════════════ */
static DialogueModifiers compute_dialogue_modifiers(const Soul& s, const Bond* bond) {
    DialogueModifiers dm = {};

    float bond_str = bond ? bond->bond_strength    : 0.0f;
    float trust    = bond ? bond->trust            : 0.0f;
    float cryst    = bond ? bond->crystallized_floor : 0.0f;

    /* Tone (mirrors BehaviorTone logic) */
    float krodha  = s.rasa[RASA_KRODHA], dvesha = s.rasa[RASA_DVESHA];
    float fear    = s.rasa[RASA_BHAYANAKA];
    float love    = s.rasa[RASA_SHRINGARA], karuna = s.rasa[RASA_KARUNA];
    float shoka   = s.rasa[RASA_SHOKA];

    /* Same net-score tone as behavior hint — bond/trust moderates hostility */
    float tone_net = (love + karuna) * 0.2f
                   + trust * 2.0f + bond_str * 0.5f + cryst * 0.5f
                   - (krodha + dvesha) * 1.5f - fear * 0.5f;
    if      (tone_net > 0.25f && cryst > 0.0f)  dm.tone = DIALOGUE_TONE_FAMILIAR;
    else if (tone_net > 0.08f)                   dm.tone = DIALOGUE_TONE_WARM;
    else if (tone_net > -0.04f)                  dm.tone = DIALOGUE_TONE_NEUTRAL;
    else if (tone_net > -0.20f)                  dm.tone = DIALOGUE_TONE_COLD;
    else                                         dm.tone = DIALOGUE_TONE_HOSTILE;

    /* Formality: driven by bond depth and dvesha (post-betrayal = formal/distant) */
    if (cryst > 0.5f && bond_str > 3.0f && dvesha < 0.2f)
        dm.formality = FORMALITY_INTIMATE;
    else if (bond_str > 1.0f && dvesha < 0.3f)
        dm.formality = FORMALITY_CASUAL;
    else if (dvesha > 0.5f || bond_str < 0.1f)
        dm.formality = FORMALITY_DISTANT;
    else
        dm.formality = FORMALITY_FORMAL;

    /* Emotional colour: dominant felt experience.
     * PEACEFUL is the fallback — it wins only when all others are below
     * the noise floor (i.e., the soul is genuinely undisturbed). This
     * prevents high resting shanta from always overriding active emotion. */
    float col_scores[7] = {};
    col_scores[EMOTIONAL_COLOR_FEARFUL]    = fear * 5.0f;
    col_scores[EMOTIONAL_COLOR_GRIEVING]   = shoka * 5.0f + s.rasa[RASA_VISHADA] * 2.0f;
    col_scores[EMOTIONAL_COLOR_JOYFUL]     = s.rasa[RASA_HASYA] * 5.0f + love * 2.0f;
    col_scores[EMOTIONAL_COLOR_ANGRY]      = krodha * 4.0f + dvesha * 3.0f;
    col_scores[EMOTIONAL_COLOR_PEACEFUL]   = 0.0f;  /* fallback, set below */
    col_scores[EMOTIONAL_COLOR_PROTECTIVE] = s.rasa[RASA_RAUDRA] * 3.0f + karuna * 2.5f
                                           + (bond_str > 1.0f ? 0.1f : 0.0f);
    col_scores[EMOTIONAL_COLOR_WARY]       = s.rasa[RASA_UDVEGA] * 3.5f + fear * 2.5f;

    int best_col = EMOTIONAL_COLOR_PEACEFUL;
    float best_col_score = 0.0f;
    for (int i = 0; i < 7; ++i) {
        if (i == EMOTIONAL_COLOR_PEACEFUL) continue;
        if (col_scores[i] > best_col_score) { best_col_score = col_scores[i]; best_col = i; }
    }
    /* Use PEACEFUL when no other emotion exceeds the noise floor */
    if (best_col_score < 0.01f) best_col = EMOTIONAL_COLOR_PEACEFUL;
    dm.emotional_color = static_cast<DialogueEmotionalColor>(best_col);

    /* Tags — score a pool, pick top 3.
     * "serene" and "distant" are scaled to rasa DEVIATIONS so they don't
     * overwhelm emotion-specific tags just because shanta is high at baseline. */
    struct TagScore { const char* name; float score; };
    TagScore pool[] = {
        { "wary",       s.rasa[RASA_UDVEGA] * 3.5f + fear * 2.5f },
        { "grieving",   shoka               * 5.0f + s.rasa[RASA_VISHADA] * 2.0f },
        { "protective", s.rasa[RASA_RAUDRA] * 3.0f + karuna * 2.5f
                       + (bond_str > 1.0f ? 0.1f : 0.0f) },
        { "joyful",     s.rasa[RASA_HASYA]  * 5.0f + love * 2.0f },
        { "fearful",    fear                * 5.0f },
        { "hostile",    krodha              * 4.0f + dvesha * 3.0f },
        { "tender",     love                * 4.0f + karuna * 2.0f + trust * 0.3f },
        /* "distant" and "serene" only score above their resting baseline */
        { "distant",    std::max(0.0f, (bond_str < 0.1f ? 0.1f : 0.0f)
                               + (1.0f - trust) * 0.1f - love * 0.5f) },
        { "curious",    s.rasa[RASA_ADBHUTA]  * 4.5f + s.rasa[RASA_VEERA] * 1.0f },
        { "serene",     std::max(0.0f, (s.rasa[RASA_SHANTA] - 0.95f) * 10.0f
                               + s.gunas[GUNA_SATTVA] * 0.05f) },
        { "despairing", s.rasa[RASA_VISHADA]  * 4.5f },
        { "courageous", s.rasa[RASA_VEERA]    * 4.0f + s.rasa[RASA_RAUDRA] * 1.5f },
        { "trusting",   trust                 * 0.9f + love * 0.2f },
        { "suspicious", dvesha                * 3.0f + s.rasa[RASA_MATSARYA] * 2.0f
                       - trust * 0.5f },
    };
    constexpr int POOL_SZ = static_cast<int>(sizeof(pool) / sizeof(pool[0]));

    for (int i = 0; i < POOL_SZ; ++i)
        if (pool[i].score < 0.0f) pool[i].score = 0.0f;

    /* Simple in-place selection sort for top 3 */
    char*  out_tags[3]    = { dm.tag0, dm.tag1, dm.tag2 };
    float* out_weights[3] = { &dm.tag0_weight, &dm.tag1_weight, &dm.tag2_weight };
    for (int slot = 0; slot < 3; ++slot) {
        int best = slot;
        for (int j = slot + 1; j < POOL_SZ; ++j)
            if (pool[j].score > pool[best].score) best = j;
        TagScore tmp = pool[slot]; pool[slot] = pool[best]; pool[best] = tmp;
        std::strncpy(out_tags[slot], pool[slot].name, 31);
        out_tags[slot][31] = '\0';
        float w = pool[slot].score;
        *out_weights[slot] = w > 1.0f ? 1.0f : w;
    }

    return dm;
}

/* ═══════════════════════════════════════════════════════════════
 * extern "C" API  —  thin wrappers over World methods
 * ═══════════════════════════════════════════════════════════════ */
extern "C" {

const char* samskriti_version() {
    return "0.3.0";
}

void samskriti_init(uint64_t rng_seed) {
    g_world.clear();
    g_rng.reseed(rng_seed);
}

void samskriti_shutdown() {
    g_world.clear();
}

SoulConfig samskriti_default_config() {
    SoulConfig cfg = {};
    cfg.parent_a_id = -1;
    cfg.parent_b_id = -1;
    /* initial_rasa / initial_gunas left zero → soul_init uses defaults */
    return cfg;
}

int create_soul(SoulConfig config) {
    return g_world.create_soul(config);
}

void destroy_soul(int soul_id) {
    g_world.destroy_soul(soul_id);
}

int get_soul_count() {
    int n = 0;
    for (const Soul& s : g_world.souls) if (s.is_alive) ++n;
    return n;
}

void set_soul_position(int soul_id, float x, float y, float z) {
    g_world.set_position(soul_id, x, y, z);
}

void inject_event(int soul_id, const char* event_type,
                  float intensity, int source_id) {
    g_world.inject_event(soul_id, event_type, intensity, source_id);
}

void tick(float dt) {
    g_world.tick(dt);
}

SoulState get_state(int soul_id) {
    Soul* s = g_world.find_soul(soul_id);
    if (!s) { SoulState st = {}; st.id = -1; return st; }
    return soul_get_state(*s);
}

BondInfo get_bond(int soul_a_id, int soul_b_id) {
    Bond* b = g_world.find_bond(soul_a_id, soul_b_id);
    if (!b) { BondInfo bi = {}; bi.soul_a = -1; bi.soul_b = -1; return bi; }
    return bond_get_info(*b);
}

void save_world(const char* filepath) {
    g_world.save(filepath);
}

void load_world(const char* filepath) {
    g_world.load(filepath);
}

uint64_t get_world_hash() {
    return g_world.hash();
}

BehaviorHint get_behavior_hint(int soul_id, int target_id) {
    Soul* s = g_world.find_soul(soul_id);
    if (!s || !s->is_alive) { return BehaviorHint{}; }
    Bond* bond = (target_id >= 0) ? g_world.find_bond(soul_id, target_id) : nullptr;
    return compute_behavior_hint(*s, bond);
}

DialogueModifiers get_dialogue_modifiers(int soul_id, int target_id) {
    Soul* s = g_world.find_soul(soul_id);
    if (!s || !s->is_alive) { return DialogueModifiers{}; }
    Bond* bond = (target_id >= 0) ? g_world.find_bond(soul_id, target_id) : nullptr;
    return compute_dialogue_modifiers(*s, bond);
}

} /* extern "C" */
