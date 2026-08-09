/**
 * samskriti.h  —  Standalone Emotional-State Substrate
 * =====================================================
 * Engine-agnostic C library.  No rendering, no LLM, no Godot types.
 * Link libsamskriti.a (static) or libsamskriti.so/.dylib (shared).
 * C++17 implementation, C-compatible public surface (extern "C").
 *
 * Core guarantee: behavior emerges from continuous state, not rules.
 * Two souls with identical config but different state histories will
 * respond measurably differently to the same inject_event() call.
 *
 * Determinism guarantee: save_world() / load_world() are bit-identical.
 * get_world_hash() before save == get_world_hash() after load, always.
 */

#pragma once
#ifndef SAMSKRITI_H
#define SAMSKRITI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Version ──────────────────────────────────────────────────── */

#define SAMSKRITI_VERSION_MAJOR 0
#define SAMSKRITI_VERSION_MINOR 3
#define SAMSKRITI_VERSION_PATCH 0

const char* samskriti_version(void);


/* ── Rasa (Emotional-State Vector) ───────────────────────────── */
/*
 * 23-element float vector [0..1] per rasa.
 * Organised into four groups from the Natyashastra taxonomy.
 * Access by RASA_* index into SoulState.rasa[].
 *
 * IMPORTANT — compound rasas (RASA_VISHADA, RASA_TITIKSHA, RASA_VAIRAGYA)
 * are NEVER injected directly.  They emerge from sustained combinations
 * during tick().  Passing them to inject_event() is a no-op.
 */

#define RASA_COUNT 23

/* Group 1: Navarasas — 9 primary aesthetic emotions */
#define RASA_SHANTA       0   /* peace, equanimity — the equilibrium state */
#define RASA_SHRINGARA    1   /* love, beauty, attraction */
#define RASA_VEERA        2   /* heroism, courage */
#define RASA_KARUNA       3   /* compassion, empathy */
#define RASA_ADBHUTA      4   /* wonder, curiosity */
#define RASA_BHAYANAKA    5   /* fear */
#define RASA_BIBHATSA     6   /* disgust, revulsion */
#define RASA_HASYA        7   /* joy, playfulness */
#define RASA_RAUDRA       8   /* righteous fury (distinct from destructive krodha) */

/* Group 2: Arishadvarga — 6 enemies of the mind */
#define RASA_KAMA         9   /* desire, craving */
#define RASA_KRODHA      10   /* destructive wrath */
#define RASA_LOBHA       11   /* greed, possessiveness */
#define RASA_MOHA        12   /* delusion, attachment to illusion */
#define RASA_MADA        13   /* pride, arrogance */
#define RASA_MATSARYA    14   /* envy */

/* Group 3: Directed emotions — targeted at a specific source soul */
#define RASA_SHOKA       15   /* personal grief from a specific loss */
#define RASA_IRSHYA      16   /* jealousy toward a rival bond */
#define RASA_DVESHA      17   /* crystallised aversion / hatred */
#define RASA_ABHIMANA    18   /* wounded pride */
#define RASA_UDVEGA      19   /* chronic anxiety (sustained bhayanaka converts here) */

/* Group 4: Emergent compounds — arise from tick(), never from inject_event() */
#define RASA_VISHADA     20   /* despair: karuna + moha + isolation */
#define RASA_TITIKSHA    21   /* endurance under suffering: veera + karuna */
#define RASA_VAIRAGYA    22   /* transcendent detachment: shanta + high sattva */


/* ── Guna (Quality of Consciousness) ────────────────────────── */
/*
 * Triguna indices into SoulState.gunas[].
 * Always sums to 1.0.  Driven by rasa history and proximity signals.
 * Tamas slows guna change — a tamasic soul resists consciousness shifts.
 */
#define GUNA_SATTVA  0   /* clarity, harmony, luminosity */
#define GUNA_RAJAS   1   /* passion, agitation, movement */
#define GUNA_TAMAS   2   /* inertia, heaviness, resistance */


/* ── Vasana (Latent Behavioral Tendency) ────────────────────── */
/*
 * 8-element vector aggregated from accumulated samskaras.
 * Values [-1..+1]: negative = suppress, positive = amplify.
 * These feed into buddhi weighting but do not dictate action.
 */
#define VASANA_COUNT           8
#define VASANA_SEEK_SOULS      0
#define VASANA_AVOID_SOULS     1
#define VASANA_EXPLORE         2
#define VASANA_HOARD           3   /* accumulate / store */
#define VASANA_TEACH           4
#define VASANA_REST            5
#define VASANA_GUARD_TERRITORY 6
#define VASANA_FOLLOW_BONDED   7


/* ── BuddhiAction (Current Behavioral Inclination) ──────────── */
/*
 * Derived from the soul's rasa + guna state at the end of each tick().
 * Not a rule ("if attacked → flee").  A function: high bhayanaka and
 * low veera will weight the output toward BUDDHI_FLEE, but a soul with
 * a strong bonded partner nearby may still choose BUDDHI_FOLLOW_BOND.
 */
typedef enum {
    BUDDHI_WANDER          = 0,
    BUDDHI_SEEK_SOUL       = 1,
    BUDDHI_FLEE            = 2,
    BUDDHI_FOLLOW_BOND     = 3,
    BUDDHI_REST            = 4,
    BUDDHI_TEACH           = 5,
    BUDDHI_GUARD_TERRITORY = 6,
    BUDDHI_HOARD           = 7,
    BUDDHI_BOND_WITH       = 8,
} BuddhiAction;


/* ── SoulConfig ──────────────────────────────────────────────── */
/*
 * Passed to create_soul().  All fields are optional.
 *
 * Zero-initialise with samskriti_default_config() to get sensible
 * defaults (shanta=0.5, gunas={0.33,0.34,0.33}, no parents).
 * Then override only the fields you care about.
 */
typedef struct {
    float    position[3];             /* abstract spatial coords (x, y, z) */

    /* Initial emotional state.  All zeros → use defaults (shanta=0.5). */
    float    initial_rasa[RASA_COUNT];

    /* Initial guna ratios.  All zeros → use defaults (0.33 each). */
    /* Must sum to 1.0 if non-zero; normalized automatically if not. */
    float    initial_gunas[3];

    /* Lineage.  Set to -1 if not applicable. */
    int      parent_a_id;
    int      parent_b_id;
    int      generation;              /* 0 = first generation */

    /* Rina polarity: [-1..1].  -1 = pure debtor, 0 = neutral, +1 = pure creditor. */
    /* Inherited from parents during create_soul() if parent ids are set. */
    float    karma_rina;

} SoulConfig;


/* ── SoulState ───────────────────────────────────────────────── */
/*
 * Snapshot returned by get_state().  Read-only from the caller's side.
 * Mutate only via inject_event() and tick().
 */
typedef struct {
    int          id;
    int          is_alive;             /* 1 = alive, 0 = dead */

    float        rasa[RASA_COUNT];     /* emotional state vector [0..1] */
    float        gunas[3];             /* sattva, rajas, tamas [0..1], sum=1.0 */

    int          dominant_rasa;        /* RASA_* index of the highest value */
    float        identity_coherence;   /* [0..1] — stability of emotional identity */

    float        vasanas[VASANA_COUNT];/* behavioral tendency weights [-1..1] */
    BuddhiAction buddhi_action;        /* current behavioral inclination */
    int          buddhi_target_id;     /* target soul id, or -1 if none */

    float        prana;                /* [0..1] vital energy; <0.2 forces rest */
    float        health;               /* [0..1]; critical illness below 0.2 */

    int          age;                  /* simulation ticks since birth */
    int          generation;           /* lineage depth */
    float        karma_rina;           /* [-1..1] debtor/creditor polarity */

    int          samskara_count;       /* accumulated impression count (max 40) */

    /**
     * Pre-computed disposition toward the source of the last inject_event() call.
     * Positive = trust / warmth.  Negative = fear / hostility.  Range [-1.0, 1.0].
     * 0.0 if no event has been injected yet, or if source_id was -1.
     *
     * Derived from: bond_strength, trust, dvesha toward source, and the soul's
     * current bhayanaka/shringara balance at the moment of the last event.
     * Lets the host engine read NPC→player affect in a single get_state() call
     * without a separate get_bond() lookup.
     */
    float        disposition_toward_source;
} SoulState;


/* ── BondInfo ────────────────────────────────────────────────── */
/*
 * Bond between two souls.  Returned by get_bond().
 * Bond strength grows from proximity + emotional resonance over time.
 * crystallized_floor is a permanent floor the bond cannot decay below —
 * earned after BOND_CRYSTALLIZE_DAYS days above BOND_CRYSTALLIZE_THRESHOLD.
 */
typedef struct {
    int   soul_a;
    int   soul_b;
    float bond_strength;        /* accumulated bond [0..∞, crystallises ~3.0+] */
    float trust;                /* [0..1] */
    float familiarity;          /* [0..1] */
    float emotional_resonance;  /* [0..1] — similarity of current rasa states */
    float crystallized_floor;   /* permanent minimum (0 = not yet crystallised) */
    float peak_bond;            /* highest bond_strength ever reached */
    int   interaction_count;
} BondInfo;


/* ── BehaviorHint ────────────────────────────────────────────── */
/*
 * Returned by get_behavior_hint().
 * Tells you what this soul is inclined to do toward a specific target,
 * derived from current rasa + bond state.  Not a command — a read-only
 * signal for the host engine to act on or ignore.
 */
typedef enum {
    BEHAVIOR_APPROACH_WARM    = 0,  /* move toward, open affect */
    BEHAVIOR_APPROACH_NEUTRAL = 1,  /* move toward, guarded */
    BEHAVIOR_RETREAT_WARY     = 2,  /* back away, low-level wariness */
    BEHAVIOR_RETREAT_FEARFUL  = 3,  /* flee, fear-dominant */
    BEHAVIOR_IGNORE           = 4,  /* no strong pull in either direction */
    BEHAVIOR_AGGRESS          = 5,  /* hostile confrontation */
    BEHAVIOR_MOURN            = 6,  /* grief-driven stillness */
    BEHAVIOR_REST             = 7,  /* exhaustion / vishada — internal */
} BehaviorAction;

typedef enum {
    TONE_HOSTILE  = 0,
    TONE_COLD     = 1,
    TONE_NEUTRAL  = 2,
    TONE_WARM     = 3,
    TONE_FAMILIAR = 4,  /* crystallised bond — beyond warm */
} BehaviorTone;

typedef struct {
    BehaviorAction action;
    float          intensity;  /* [0..1] — confidence / strength of inclination */
    BehaviorTone   tone;
} BehaviorHint;


/* ── DialogueModifiers ───────────────────────────────────────── */
/*
 * Returned by get_dialogue_modifiers().
 * Studios use these to route existing dialogue line pools without
 * writing any Samskriti-specific dialogue.
 * Example: tone=WARM + emotional_color=GRIEVING + tag "tender" routes
 * to a line that is warm but carries grief.
 */
typedef enum {
    DIALOGUE_TONE_HOSTILE  = 0,
    DIALOGUE_TONE_COLD     = 1,
    DIALOGUE_TONE_NEUTRAL  = 2,
    DIALOGUE_TONE_WARM     = 3,
    DIALOGUE_TONE_FAMILIAR = 4,
} DialogueTone;

typedef enum {
    FORMALITY_DISTANT  = 0,  /* stranger or post-betrayal */
    FORMALITY_FORMAL   = 1,  /* respectful but not intimate */
    FORMALITY_CASUAL   = 2,  /* familiar acquaintance */
    FORMALITY_INTIMATE = 3,  /* crystallised bond */
} DialogueFormality;

typedef enum {
    EMOTIONAL_COLOR_FEARFUL    = 0,
    EMOTIONAL_COLOR_GRIEVING   = 1,
    EMOTIONAL_COLOR_JOYFUL     = 2,
    EMOTIONAL_COLOR_ANGRY      = 3,
    EMOTIONAL_COLOR_PEACEFUL   = 4,
    EMOTIONAL_COLOR_PROTECTIVE = 5,
    EMOTIONAL_COLOR_WARY       = 6,
} DialogueEmotionalColor;

typedef struct {
    DialogueTone           tone;
    DialogueFormality      formality;
    DialogueEmotionalColor emotional_color;
    /* Top 3 weighted descriptor tags (ASCII, null-terminated) */
    char  tag0[32];  float tag0_weight;
    char  tag1[32];  float tag1_weight;
    char  tag2[32];  float tag2_weight;
} DialogueModifiers;


/* ═══════════════════════════════════════════════════════════════
 * PUBLIC API
 * ═══════════════════════════════════════════════════════════════ */

/* ── Lifecycle ───────────────────────────────────────────────── */

/**
 * samskriti_init()
 * Initialize (or reinitialize) the world.  Call once before anything else.
 *
 * rng_seed: 0 = seed from OS entropy (non-deterministic across runs).
 *           nonzero = fully deterministic; same seed → same world.
 *           The seed is saved by save_world() and restored by load_world().
 */
void samskriti_init(uint64_t rng_seed);

/**
 * samskriti_shutdown()
 * Free all resources.  After this call the API is invalid until samskriti_init().
 */
void samskriti_shutdown(void);

/**
 * samskriti_default_config()
 * Returns a SoulConfig pre-filled with sensible defaults.
 * Override only the fields you need.
 */
SoulConfig samskriti_default_config(void);


/* ── Soul Management ─────────────────────────────────────────── */

/**
 * create_soul()
 * Spawn an agent.  Returns a stable soul id (≥0), or -1 on failure.
 *
 * If parent_a_id / parent_b_id are set in config:
 *   - karma_rina is interpolated from parents + a small RNG mutation
 *   - inherited_rasa echo is seeded from parents' death-moment rasa
 *   - generation is set to max(parent generations) + 1
 *
 * The soul starts in emotional equilibrium (shanta dominant) unless
 * initial_rasa is non-zero in config.
 */
int create_soul(SoulConfig config);

/**
 * destroy_soul()
 * Mark a soul as dead.  Bond history is preserved for lineage queries.
 * The soul id is never reused within a session.
 */
void destroy_soul(int soul_id);

/**
 * get_soul_count()
 * Returns the number of currently living souls.
 */
int get_soul_count(void);


/* ── Spatial Input ───────────────────────────────────────────── */

/**
 * set_soul_position()
 * Update a soul's spatial position.  The core uses this to compute
 * proximity for bond formation and isolation signals.
 * Call once per soul per tick, before calling tick().
 * Units are arbitrary; scale consistently with your host engine.
 */
void set_soul_position(int soul_id, float x, float y, float z);


/* ── The 5 Core Behavioral Functions ────────────────────────── */

/**
 * inject_event()
 * Inject an external event onto a soul.
 *
 * THE KEY INVARIANT: the rasa delta is NOT fixed per event_type.
 * It is computed from the soul's current rasa and guna state at the
 * moment of the call.  A high-tamas, high-bhayanaka soul and a
 * high-sattva, high-shanta soul will produce measurably different
 * state changes from the same ("attacked", 0.7) call.
 *
 * event_type (case-insensitive):
 *   "helped"          — another soul rendered aid
 *   "attacked"        — physical or social aggression
 *   "gift"            — resource or word offered freely
 *   "witnessed_death" — a known soul died nearby
 *   "abandoned"       — bonded partner moved away
 *   "betrayed"        — trust violated by source_id
 *   "reunion"         — bonded partner returned
 *   "discovery"       — novel stimulus encountered
 *
 * intensity: [0..1].  Scales the magnitude of state change.
 * source_id: soul that caused the event, or -1 for environmental.
 *
 * NOTE: RASA_VISHADA, RASA_TITIKSHA, RASA_VAIRAGYA are compound rasas
 * and are never set directly by inject_event().  They emerge via tick().
 */
void inject_event(int soul_id, const char* event_type,
                  float intensity, int source_id);

/**
 * tick()
 * Advance ALL living souls' state by dt units of simulation time.
 *
 * Per tick, for each soul:
 *   1. Rasa decay — sthayibhava (slow) and vyabhicaribhava (fast) rates,
 *      modulated by identity_coherence and tamas.  Shanta fills the space
 *      left by decaying rasas (equilibrium pull).
 *   2. Rasa coupling — continuous coupling equations between rasa pairs
 *      (e.g. high krodha suppresses shanta; high shanta damps bhayanaka).
 *   3. Compound rasa emergence — vishada / titiksha / vairagya emerge when
 *      their prerequisite rasa combinations sustain above threshold.
 *   4. Guna evolution — rasas drive guna targets; tamas slows convergence.
 *   5. Samskara accumulation — significant rasa shifts (|Δ| > threshold)
 *      are imprinted; samskaras aggregate into vasanas over time.
 *   6. Bond formation — proximity × emotional resonance × trust grows bonds.
 *      Crystallisation locks a floor after sustained threshold.
 *   7. Buddhi weighting — action inclination derived from current state.
 *   8. Prana / health / lifecycle — prana drains with activity, restores
 *      with rest; low prana elevates tamas.
 *
 * No LLM.  No rendering.  Pure continuous math.
 */
void tick(float dt);

/**
 * get_state()
 * Read a soul's current state snapshot.
 * Returns a zeroed SoulState (id = -1) if soul_id is not found.
 */
SoulState get_state(int soul_id);

/**
 * save_world()
 * Write the full world state to filepath (JSON).
 * Includes: all soul states, all bonds, the RNG state.
 * Determinism guarantee: get_world_hash() before and after a save/load
 * round-trip must be identical.
 */
void save_world(const char* filepath);

/**
 * load_world()
 * Replace current world state from filepath.
 * Existing souls are discarded.  The RNG is reseeded from the file.
 */
void load_world(const char* filepath);


/* ── Bond Query ──────────────────────────────────────────────── */

/**
 * get_bond()
 * Read the relationship record between two souls.
 * Returns a zeroed BondInfo (soul_a = soul_b = -1) if no relationship exists.
 */
BondInfo get_bond(int soul_a_id, int soul_b_id);


/* ── Determinism Verification ────────────────────────────────── */

/**
 * get_world_hash()
 * 64-bit hash of the canonical world state (all soul states + all bonds +
 * current RNG seed).  Bit-identical state → identical hash.
 *
 * Use to prove persistence:
 *   uint64_t h1 = get_world_hash();
 *   save_world("test.json");
 *   load_world("test.json");
 *   uint64_t h2 = get_world_hash();
 *   assert(h1 == h2);  // must always pass
 */
uint64_t get_world_hash(void);


/* ── Studio-readiness: Behavior Hints ───────────────────────── */

/**
 * get_behavior_hint()
 * What is soul_id inclined to do toward target_id right now?
 *
 * Derived from: soul's current rasa vector, dominant guna, and the
 * bond record between soul_id and target_id (if any).
 * target_id = -1 returns a context-free inclination.
 *
 * Not a command.  The soul may act differently based on the host engine's
 * additional constraints.  Use as an input to animation blending,
 * dialogue routing, or NPC movement goals.
 */
BehaviorHint get_behavior_hint(int soul_id, int target_id);


/* ── Studio-readiness: Dialogue Modifiers ───────────────────── */

/**
 * get_dialogue_modifiers()
 * Returns tone/formality/color and 3 weighted descriptor tags for
 * routing dialogue line pools without Samskriti-specific authoring.
 *
 * Example use:
 *   DialogueModifiers dm = get_dialogue_modifiers(npc_id, player_id);
 *   if (dm.tone == DIALOGUE_TONE_WARM && dm.formality == FORMALITY_INTIMATE)
 *       play_line("greet_close_friend");
 *   // OR: pass dm.tag0 / dm.tag1 / dm.tag2 to your dialogue selector
 *
 * tags are ASCII strings like "wary", "grieving", "protective", "joyful".
 * tag_weights are [0..1] — higher means the tag dominates the soul's state.
 */
DialogueModifiers get_dialogue_modifiers(int soul_id, int target_id);


#ifdef __cplusplus
}  /* extern "C" */
#endif
#endif /* SAMSKRITI_H */
