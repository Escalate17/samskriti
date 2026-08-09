#pragma once
#include <array>
#include "../include/samskriti.h"

static constexpr int   MAX_SAMSKARAS      = 40;
static constexpr float SAMSKARA_THRESHOLD = 0.05f;  /* min |Δrasa| to imprint */
static constexpr int   TICKS_PER_DAY      = 1800;   /* matches Godot reference */

/* Context bitmask for Samskara */
static constexpr int CTX_ALONE         = 1;
static constexpr int CTX_BONDED_NEARBY = 2;
static constexpr int CTX_STRANGER_NEAR = 4;
static constexpr int CTX_VOID_PROXIMAL = 8;

struct Samskara {
    float rasa_at_moment[RASA_COUNT];
    int   dominant_rasa;
    float valence;          /* positive = pleasant, negative = aversive */
    float strength;         /* |Δdominant_rasa| at formation */
    int   buddhi_at_moment;
    int   context_hash;
    int   source_soul_id;   /* -1 if environmental */
};

struct Soul {
    int  id        = -1;
    bool is_alive  = false;

    /* Spatial */
    float position[3] = {0, 0, 0};

    /* Emotional state */
    float rasa[RASA_COUNT]  = {};
    float gunas[3]          = {0.33f, 0.34f, 0.33f};
    float vasanas[VASANA_COUNT] = {};

    /* Samskaras — ring buffer */
    Samskara samskara_ring[MAX_SAMSKARAS] = {};
    int      samskara_head  = 0;
    int      samskara_count = 0;

    /* Identity */
    float identity_coherence = 0.3f;

    /* Vitality */
    float prana  = 1.0f;
    float health = 1.0f;

    /* Lifecycle */
    int   age        = 0;
    int   generation = 0;
    float karma_rina = 0.0f;

    /* Lineage */
    int   parent_a_id = -1;
    int   parent_b_id = -1;
    float inherited_rasa[RASA_COUNT] = {};  /* subtle ancestral pull */

    /* Buddhi */
    BuddhiAction buddhi_action    = BUDDHI_WANDER;
    int          buddhi_target_id = -1;
    float        buddhi_timer     = 0.0f;

    /* Compound rasa temporal accumulators */
    float shoka_sustained  = 0.0f; /* days above 0.8 shoka → vishada */

    /* Last injected event */
    int   last_event_source_id          = -1;
    float disposition_toward_source     = 0.0f;

    /* Perception signals (written by World each tick before soul::tick) */
    float isolation       = 0.0f;
    float bonded_presence = 0.0f;
    float void_proximity  = 0.0f;
    float world_certainty = 1.0f;

    /* Previous rasa snapshot for samskara delta detection */
    float prev_rasa[RASA_COUNT] = {};

    /* Spontaneous rasa event countdown (in sim seconds) */
    float rasa_event_timer = 0.0f;
};

/* ── Soul math (implemented in soul.cpp) ────────────────────── */

void soul_init(Soul& s, const SoulConfig& cfg, int id);
void soul_tick(Soul& s, float dt);

/* State-dependent event injection.  No lookup table — the delta for
 * each rasa is computed from the soul's current rasa+guna state. */
void soul_inject_event(Soul& s, const char* event_type,
                       float intensity, int source_id,
                       float bond_strength, float trust_to_source,
                       float dvesha_toward_source);

/* Recompute vasanas from samskara ring buffer. */
void soul_recompute_vasanas(Soul& s);

/* Derive buddhi action from current state + nearest bonded soul distance. */
void soul_update_buddhi(Soul& s, float nearest_bonded_dist,
                        float nearest_any_dist, int nearest_bonded_id);

/* Derive disposition [-1,1] toward a specific soul given relationship data. */
float soul_disposition(const Soul& s,
                       float bond_strength, float trust,
                       float dvesha);

/* Build a SoulState snapshot from internal struct. */
SoulState soul_get_state(const Soul& s);
