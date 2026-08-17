#include "soul.hpp"
#include "rng.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <string>

/* ── helpers ─────────────────────────────────────────────────── */

static inline float clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}
static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

/* ── Rasa decay tables (from Natyashastra taxonomy) ─────────── */

static const float STHAYI_DECAY[RASA_COUNT] = {
    /* SHANTA    */ 0.003f,
    /* SHRINGARA */ 0.010f,
    /* VEERA     */ 0.012f,
    /* KARUNA    */ 0.010f,
    /* ADBHUTA   */ 0.018f,
    /* BHAYANAKA */ 0.025f,
    /* BIBHATSA  */ 0.020f,
    /* HASYA     */ 0.020f,
    /* RAUDRA    */ 0.022f,
    /* KAMA      */ 0.012f,
    /* KRODHA    */ 0.018f,
    /* LOBHA     */ 0.010f,
    /* MOHA      */ 0.006f,
    /* MADA      */ 0.012f,
    /* MATSARYA  */ 0.015f,
    /* SHOKA     */ 0.005f,
    /* IRSHYA    */ 0.018f,
    /* DVESHA    */ 0.004f,
    /* ABHIMANA  */ 0.015f,
    /* UDVEGA    */ 0.008f,
    /* VISHADA   */ 0.003f,
    /* TITIKSHA  */ 0.007f,
    /* VAIRAGYA  */ 0.006f,
};

/* Decay for the four emergent rasas, indexed from RASA_UDVEGA. Roughly a tenth of
   each one's formation coefficient, so a sustained condition accumulates but a
   lapsed one still erodes. See the note in the decay loop for why these cannot
   share STHAYI_DECAY. */
static const float EMERGENT_DECAY[4] = {
    /* UDVEGA    */ 0.0004f,   /* forms at bhayanaka * 0.004      */
    /* VISHADA   */ 0.0008f,   /* forms at (cond - 0.5) * 0.008   */
    /* TITIKSHA  */ 0.0006f,   /* forms at (cond - 0.4) * 0.006   */
    /* VAIRAGYA  */ 0.0005f,   /* forms at (cond - 0.5) * 0.005   */
};

/* ── Rasa coupling coefficients ─────────────────────────────── */
/* These are mathematical relationships between rasas, not rules.
 * They express: "sustained high X pulls Y in direction sign."     */

struct Coupling { int source; int target; float rate; };

static const Coupling COUPLINGS[] = {
    /* Krodha suppresses Shanta */
    { RASA_KRODHA,    RASA_SHANTA,    -0.012f },
    /* Shanta damps Bhayanaka */
    { RASA_SHANTA,    RASA_BHAYANAKA, -0.010f },
    /* Shanta damps Krodha */
    { RASA_SHANTA,    RASA_KRODHA,    -0.008f },
    /* Karuna damps Krodha (compassion moderates wrath) */
    { RASA_KARUNA,    RASA_KRODHA,    -0.007f },
    /* Sustained Bhayanaka breeds Udvega (chronic anxiety) */
    { RASA_BHAYANAKA, RASA_UDVEGA,     0.004f },
    /* Bhayanaka suppresses Veera */
    { RASA_BHAYANAKA, RASA_VEERA,     -0.006f },
    /* Veera damps Bhayanaka (courage counters fear) */
    { RASA_VEERA,     RASA_BHAYANAKA, -0.008f },
    /* Shringara raises Hasya (love brings joy) */
    { RASA_SHRINGARA, RASA_HASYA,      0.004f },
    /* Hasya damps Bhayanaka */
    { RASA_HASYA,     RASA_BHAYANAKA, -0.006f },
    /* Moha breeds Lobha (delusion feeds greed) */
    { RASA_MOHA,      RASA_LOBHA,      0.003f },
    /* Krodha feeds Moha (unresolved wrath → delusion) */
    { RASA_KRODHA,    RASA_MOHA,       0.004f },
    /* Raudra suppresses Krodha (righteous anger channels destructive wrath) */
    { RASA_RAUDRA,    RASA_KRODHA,    -0.006f },
    /* Mada breeds Abhimana (pride is fragile) */
    { RASA_MADA,      RASA_ABHIMANA,   0.005f },
    /* Dvesha feeds Krodha */
    { RASA_DVESHA,    RASA_KRODHA,     0.005f },
    /* Matsarya damps Shringara (envy corrodes love) */
    { RASA_MATSARYA,  RASA_SHRINGARA, -0.006f },
    /* Vairagya damps Kama and Lobha */
    { RASA_VAIRAGYA,  RASA_KAMA,      -0.008f },
    { RASA_VAIRAGYA,  RASA_LOBHA,     -0.007f },
    /* Titiksha feeds Veera */
    { RASA_TITIKSHA,  RASA_VEERA,      0.005f },
};
static constexpr int N_COUPLINGS = static_cast<int>(sizeof(COUPLINGS)/sizeof(COUPLINGS[0]));

/* ── Event perturbation profiles ────────────────────────────── */
/*
 * Each event lists candidate rasa dimensions and a direction (+1 / -1).
 * The actual delta is computed entirely from current soul state:
 *   delta = dir × intensity × governing_guna × (cur_rasa + 0.1) × guna_scale × rel_amp × dvesha_mod
 *
 * Emergent rasas (VISHADA/TITIKSHA/VAIRAGYA) are excluded — tick() only.
 */
struct RasaCandidate { int idx; int direction; };

struct EventProfile {
    const char*   name;
    RasaCandidate candidates[8];
    int           n_candidates;
};

/* Maps each rasa to its governing aspect of consciousness.
 * The governing guna value becomes the primary sensitivity multiplier. */
static float governing_guna_value(int rasa_idx, const Soul& s) {
    switch (rasa_idx) {
        /* SATTVA: openness, compassion, wonder, beauty, vulnerability */
        case RASA_SHANTA: case RASA_SHRINGARA: case RASA_KARUNA:
        case RASA_ADBHUTA: case RASA_HASYA: case RASA_BHAYANAKA:
        case RASA_TITIKSHA: case RASA_VAIRAGYA:
            return s.gunas[GUNA_SATTVA];
        /* RAJAS: action, passion, courage, wrath, pride, envy */
        case RASA_VEERA: case RASA_RAUDRA: case RASA_KRODHA:
        case RASA_KAMA: case RASA_MADA: case RASA_MATSARYA:
        case RASA_ABHIMANA: case RASA_IRSHYA:
            return s.gunas[GUNA_RAJAS];
        /* TAMAS: heaviness, delusion, grief, aversion, despair */
        case RASA_BIBHATSA: case RASA_MOHA: case RASA_LOBHA:
        case RASA_SHOKA: case RASA_DVESHA: case RASA_UDVEGA:
        case RASA_VISHADA:
            return s.gunas[GUNA_TAMAS];
        default:
            return 1.0f / 3.0f;
    }
}

static const EventProfile EVENT_PROFILES[] = {
    { "helped",
      {{ RASA_SHRINGARA, +1 }, { RASA_KARUNA,    +1 },
       { RASA_ADBHUTA,  +1 }, { RASA_BHAYANAKA, -1 },
       { RASA_MOHA,     -1 }}, 5 },
    { "attacked",
      {{ RASA_BHAYANAKA, +1 }, { RASA_RAUDRA,    +1 },
       { RASA_KRODHA,   +1 }, { RASA_ABHIMANA,  +1 },
       { RASA_SHRINGARA,-1 }}, 5 },
    { "gift",
      {{ RASA_SHRINGARA, +1 }, { RASA_HASYA,     +1 },
       { RASA_ADBHUTA,  +1 }, { RASA_LOBHA,     -1 }}, 4 },
    { "witnessed_death",
      {{ RASA_KARUNA,    +1 }, { RASA_BHAYANAKA, +1 },
       { RASA_SHOKA,    +1 }, { RASA_HASYA,     -1 },
       { RASA_SHRINGARA,-1 }}, 5 },
    { "abandoned",
      {{ RASA_SHOKA,     +1 }, { RASA_BHAYANAKA, +1 },
       { RASA_KARUNA,   +1 }, { RASA_SHRINGARA, -1 },
       { RASA_HASYA,    -1 }}, 5 },
    { "betrayed",
      {{ RASA_DVESHA,    +1 }, { RASA_KRODHA,    +1 },
       { RASA_ABHIMANA, +1 }, { RASA_SHRINGARA, -1 }}, 4 },
    { "reunion",
      {{ RASA_SHRINGARA, +1 }, { RASA_HASYA,     +1 },
       { RASA_KARUNA,   +1 }, { RASA_SHOKA,     -1 },
       { RASA_BHAYANAKA,-1 }}, 5 },
    { "discovery",
      {{ RASA_ADBHUTA,   +1 }, { RASA_HASYA,     +1 },
       { RASA_VEERA,    +1 }, { RASA_MOHA,      -1 }}, 4 },
    /* ── v0.3.0 environmental / social events ─────────────────── */
    { "entered_sacred_space",
      {{ RASA_ADBHUTA,   +1 }, { RASA_KARUNA,    +1 },
       { RASA_KRODHA,   -1 }, { RASA_KAMA,      -1 },
       { RASA_MOHA,     -1 }}, 5 },
    { "saw_threat",
      {{ RASA_BHAYANAKA, +1 }, { RASA_RAUDRA,    +1 },
       { RASA_UDVEGA,   +1 }, { RASA_HASYA,     -1 }}, 4 },
    { "received_gift",
      {{ RASA_SHRINGARA, +1 }, { RASA_HASYA,     +1 },
       { RASA_KARUNA,   +1 }, { RASA_LOBHA,     -1 },
       { RASA_DVESHA,   -1 }}, 5 },
    { "celebrated_with",
      {{ RASA_HASYA,     +1 }, { RASA_SHRINGARA, +1 },
       { RASA_KARUNA,   +1 }, { RASA_BHAYANAKA, -1 },
       { RASA_SHOKA,    -1 }}, 5 },
};
static constexpr int N_EVENT_PROFILES =
    static_cast<int>(sizeof(EVENT_PROFILES)/sizeof(EVENT_PROFILES[0]));

static const EventProfile* find_profile(const char* event_type) {
    std::string et(event_type);
    for (char& c : et) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (int i = 0; i < N_EVENT_PROFILES; ++i) {
        if (et == EVENT_PROFILES[i].name) return &EVENT_PROFILES[i];
    }
    return nullptr;
}

/* ── Vasana contribution map ─────────────────────────────────── */
/* Maps context + valence to vasana index tendencies.
 * This too is continuous aggregation, not a decision tree.          */

static void accumulate_vasana(float vasanas[VASANA_COUNT],
                               const Samskara& s) {
    float str = s.strength * 0.1f;
    float val = s.valence;

    if ((s.context_hash & CTX_BONDED_NEARBY) && val > 0.0f) {
        vasanas[VASANA_SEEK_SOULS]    += val * str;
        vasanas[VASANA_FOLLOW_BONDED] += val * str * 0.5f;
    }
    if ((s.context_hash & CTX_STRANGER_NEAR) && val < 0.0f) {
        vasanas[VASANA_AVOID_SOULS]   += std::abs(val) * str;
    }
    if ((s.context_hash & CTX_ALONE) && val > 0.0f) {
        vasanas[VASANA_EXPLORE]       += val * str;
    }
    if ((s.context_hash & CTX_ALONE) && val < 0.0f) {
        vasanas[VASANA_SEEK_SOULS]    += std::abs(val) * str * 0.5f;
    }
    if (s.context_hash & CTX_VOID_PROXIMAL) {
        vasanas[VASANA_REST]          += str * 0.3f;
    }
    if (s.buddhi_at_moment == BUDDHI_TEACH && val > 0.0f) {
        vasanas[VASANA_TEACH]         += val * str;
    }
    if (s.buddhi_at_moment == BUDDHI_HOARD && val > 0.0f) {
        vasanas[VASANA_HOARD]         += val * str;
    }
    if (s.buddhi_at_moment == BUDDHI_GUARD_TERRITORY && val > 0.0f) {
        vasanas[VASANA_GUARD_TERRITORY] += val * str;
    }
}

/* ── Rasa positivity lookup (for samskara valence) ──────────── */
static const float RASA_VALENCE[RASA_COUNT] = {
    +0.8f,  /* shanta     — positive */
    +0.7f,  /* shringara  — positive */
    +0.6f,  /* veera      — positive */
    +0.5f,  /* karuna     — bittersweet, net positive */
    +0.6f,  /* adbhuta    — positive */
    -0.7f,  /* bhayanaka  — negative */
    -0.6f,  /* bibhatsa   — negative */
    +0.7f,  /* hasya      — positive */
    -0.3f,  /* raudra     — costly but purposeful, mild negative */
    -0.2f,  /* kama       — can be pleasant but ultimately draining */
    -0.8f,  /* krodha     — negative */
    -0.5f,  /* lobha      — negative */
    -0.6f,  /* moha       — negative */
    -0.4f,  /* mada       — negative */
    -0.6f,  /* matsarya   — negative */
    -0.7f,  /* shoka      — negative */
    -0.5f,  /* irshya     — negative */
    -0.9f,  /* dvesha     — very negative */
    -0.5f,  /* abhimana   — negative */
    -0.7f,  /* udvega     — negative */
    -0.9f,  /* vishada    — very negative */
    +0.4f,  /* titiksha   — net positive (growth through suffering) */
    +0.9f,  /* vairagya   — very positive */
};

/* ═══════════════════════════════════════════════════════════════
 * soul_init
 * ═══════════════════════════════════════════════════════════════ */
void soul_init(Soul& s, const SoulConfig& cfg, int id) {
    s = {};  /* zero-initialise */
    s.id       = id;
    s.is_alive = true;

    std::memcpy(s.position, cfg.position, sizeof(s.position));

    /* Gunas */
    float gs = cfg.initial_gunas[0] + cfg.initial_gunas[1] + cfg.initial_gunas[2];
    if (gs > 1e-6f) {
        s.gunas[0] = cfg.initial_gunas[0] / gs;
        s.gunas[1] = cfg.initial_gunas[1] / gs;
        s.gunas[2] = cfg.initial_gunas[2] / gs;
    } else {
        s.gunas[0] = 0.33f; s.gunas[1] = 0.34f; s.gunas[2] = 0.33f;
    }

    /* Rasa */
    float rsum = 0.0f;
    for (int i = 0; i < RASA_COUNT; ++i) rsum += cfg.initial_rasa[i];
    if (rsum > 1e-6f) {
        for (int i = 0; i < RASA_COUNT; ++i)
            s.rasa[i] = clamp01(cfg.initial_rasa[i]);
    } else {
        s.rasa[RASA_SHANTA] = 0.5f;
    }

    s.parent_a_id = cfg.parent_a_id;
    s.parent_b_id = cfg.parent_b_id;
    s.generation  = cfg.generation;
    s.karma_rina  = clampf(cfg.karma_rina, -1.0f, 1.0f);

    s.prana  = 1.0f;
    s.health = 1.0f;
    s.identity_coherence = 0.3f;

    s.rasa_event_timer = g_rng.range(20.0f, 60.0f);

    std::memcpy(s.prev_rasa, s.rasa, sizeof(s.rasa));
}

/* ═══════════════════════════════════════════════════════════════
 * soul_tick
 * ═══════════════════════════════════════════════════════════════ */
void soul_tick(Soul& s, float dt) {
    if (!s.is_alive) return;

    float sattva = s.gunas[GUNA_SATTVA];
    float rajas  = s.gunas[GUNA_RAJAS];
    float tamas  = s.gunas[GUNA_TAMAS];

    /* coherence_stability modulates decay: stable identity = slower decay */
    float coherence_stability = lerpf(0.5f, 1.5f, s.identity_coherence);

    /* ── 1. Rasa decay ─────────────────────────────────────── */
    /* The four emergent rasas (UDVEGA..VAIRAGYA) decay on their own, far slower,
       schedule. They are dispositions, not momentary feelings, and decaying a
       disposition at a feeling's rate is a category error with a hard consequence:
       at STHAYI_DECAY rates the decay term exceeds the maximum possible emergence
       term every tick, so three of the four could never form in any world at any
       timescale. Break-even conditions were 1.567 (titiksha), 1.700 (vairagya) and
       bhayanaka=2.0 (udvega) against inputs clamped to 1.0.

       These rates are ~1/10th of each rasa's formation coefficient, which restores
       accumulation while keeping the property that matters: they still fade when
       the conditions that grew them stop, so they must be SUSTAINED, not merely
       touched once. */
    float tamas_decay_mod = 1.0f - tamas * 0.5f;  /* tamas → slower processing */
    for (int i = 0; i < RASA_COUNT; ++i) {
        if (i == RASA_SHANTA) continue;  /* shanta handled separately */
        float base = (i >= RASA_UDVEGA) ? EMERGENT_DECAY[i - RASA_UDVEGA]
                                        : STHAYI_DECAY[i];
        float effective_decay = base * coherence_stability * tamas_decay_mod;
        s.rasa[i] = std::max(0.0f, s.rasa[i] - dt * effective_decay);
    }

    /* ── 2. Shanta equilibrium fill ────────────────────────── */
    float total_non_shanta = 0.0f;
    for (int i = 1; i < RASA_COUNT; ++i) total_non_shanta += s.rasa[i];
    s.rasa[RASA_SHANTA] = clampf(1.0f - total_non_shanta * 0.15f, 0.05f, 1.0f);

    /* ── 3. Rasa coupling equations ────────────────────────── */
    float rasa_delta[RASA_COUNT] = {};
    for (int i = 0; i < N_COUPLINGS; ++i) {
        const Coupling& c = COUPLINGS[i];
        rasa_delta[c.target] += s.rasa[c.source] * c.rate * dt;
    }
    for (int i = 0; i < RASA_COUNT; ++i)
        s.rasa[i] = clamp01(s.rasa[i] + rasa_delta[i]);

    /* ── 4. Sattvic grief: shoka × sattva → karuna ─────────── */
    float sattvic_compassion = s.rasa[RASA_SHOKA] * sattva;
    s.rasa[RASA_KARUNA] = std::min(1.0f,
        s.rasa[RASA_KARUNA] + sattvic_compassion * dt * 0.008f);

    /* ── 5. Compound rasa emergence ────────────────────────── */

    /* VISHADA (despair) */
    {
        float cond = s.rasa[RASA_KARUNA]    * 0.4f
                   + s.rasa[RASA_MOHA]      * 0.3f
                   + s.isolation             * 0.3f
                   + s.rasa[RASA_BHAYANAKA] * 0.2f;
        if (cond > 0.5f)
            s.rasa[RASA_VISHADA] = clamp01(
                s.rasa[RASA_VISHADA] + (cond - 0.5f) * 0.008f * dt);
        float joy_counter = (s.rasa[RASA_HASYA] + s.rasa[RASA_SHRINGARA]) * 0.01f * dt;
        s.rasa[RASA_VISHADA] = std::max(0.0f, s.rasa[RASA_VISHADA] - joy_counter);

        /* Shoka → Vishada pathway: personal grief sustained ≥ 3 sim days */
        if (s.rasa[RASA_SHOKA] > 0.8f) {
            s.shoka_sustained += dt / static_cast<float>(TICKS_PER_DAY);
            if (s.shoka_sustained >= 3.0f)
                s.rasa[RASA_VISHADA] = std::min(1.0f, s.rasa[RASA_VISHADA] + dt * 0.01f);
        } else {
            s.shoka_sustained = std::max(0.0f, s.shoka_sustained - dt * 0.1f);
        }
    }

    /* TITIKSHA (endurance under suffering) */
    {
        float cond = s.rasa[RASA_VEERA]  * 0.5f
                   + s.rasa[RASA_KARUNA] * 0.5f;
        if (cond > 0.4f)
            s.rasa[RASA_TITIKSHA] = clamp01(
                s.rasa[RASA_TITIKSHA] + (cond - 0.4f) * 0.006f * dt);
    }

    /* VAIRAGYA (transcendent detachment) */
    {
        float age_factor = clampf(static_cast<float>(s.age) / 50000.0f, 0.0f, 1.0f);
        float cond = s.rasa[RASA_SHANTA] * 0.3f
                   + sattva               * 0.3f
                   + age_factor           * 0.2f;
        if (s.rasa[RASA_VISHADA] > 0.3f && sattva > 0.4f) cond += 0.3f;
        if (cond > 0.5f)
            s.rasa[RASA_VAIRAGYA] = clamp01(
                s.rasa[RASA_VAIRAGYA] + (cond - 0.5f) * 0.005f * dt);
        float desire = (s.rasa[RASA_KAMA] + s.rasa[RASA_LOBHA]) * 0.008f * dt;
        s.rasa[RASA_VAIRAGYA] = std::max(0.0f, s.rasa[RASA_VAIRAGYA] - desire);
    }

    /* ── 6. Ancestral rasa pull ─────────────────────────────── */
    for (int i = 0; i < RASA_COUNT; ++i) {
        if (s.inherited_rasa[i] > 0.0f) {
            float pull = s.inherited_rasa[i] * 0.005f * dt;
            s.rasa[i] = std::min(1.0f, s.rasa[i] + pull);
        }
    }

    /* ── 7. Guna evolution ──────────────────────────────────── */
    float target_s = sattva, target_r = rajas, target_t = tamas;

    target_s += s.bonded_presence * 0.08f;
    target_t += s.void_proximity  * 0.10f;
    if (s.isolation > 0.7f) {
        if (rajas < 0.5f) target_r += 0.04f;
        else              target_t += 0.04f;
    }
    target_s += s.world_certainty                     * 0.04f;
    target_s += clampf(static_cast<float>(s.age) / 50000.0f, 0.0f, 1.0f) * 0.02f;

    /* Rasa → guna targets */
    target_s += s.rasa[RASA_ADBHUTA]  * 0.04f + s.rasa[RASA_SHANTA]   * 0.03f
              + s.rasa[RASA_VAIRAGYA] * 0.05f + s.rasa[RASA_TITIKSHA] * 0.03f;
    target_r += s.rasa[RASA_BHAYANAKA]* 0.04f + s.rasa[RASA_RAUDRA]   * 0.05f
              + s.rasa[RASA_SHRINGARA]* 0.03f + s.rasa[RASA_KAMA]     * 0.05f
              + s.rasa[RASA_KRODHA]   * 0.05f + s.rasa[RASA_MATSARYA] * 0.03f
              + s.rasa[RASA_LOBHA]    * 0.03f;
    target_t += s.rasa[RASA_KARUNA]   * 0.04f + s.rasa[RASA_BIBHATSA] * 0.05f
              + s.rasa[RASA_MOHA]     * 0.06f + s.rasa[RASA_VISHADA]  * 0.06f
              + s.rasa[RASA_MADA]     * 0.03f;

    /* Tamas resists change — tamasic soul's consciousness shifts slowly */
    float lerp_speed = clampf(0.05f * (1.0f - tamas * 0.6f), 0.008f, 0.05f);
    s.gunas[GUNA_SATTVA] = lerpf(sattva, clampf(target_s, 0.05f, 0.90f), lerp_speed);
    s.gunas[GUNA_RAJAS]  = lerpf(rajas,  clampf(target_r, 0.05f, 0.90f), lerp_speed);
    s.gunas[GUNA_TAMAS]  = lerpf(tamas,  clampf(target_t, 0.05f, 0.90f), lerp_speed);

    /* Normalise */
    float gsum = s.gunas[0] + s.gunas[1] + s.gunas[2];
    if (gsum > 1e-6f) {
        s.gunas[0] /= gsum; s.gunas[1] /= gsum; s.gunas[2] /= gsum;
    }

    /* ── 8. Identity coherence ──────────────────────────────── */
    /* Coherence grows with dominant shanta and sattva; high volatile rasas erode it */
    float emotional_volatility = 0.0f;
    for (int i = 1; i < RASA_COUNT; ++i)
        emotional_volatility += s.rasa[i];
    emotional_volatility /= static_cast<float>(RASA_COUNT - 1);
    float coherence_target = clampf(s.rasa[RASA_SHANTA] * 0.5f + s.gunas[GUNA_SATTVA] * 0.5f
                                    - emotional_volatility * 0.3f, 0.0f, 1.0f);
    s.identity_coherence = lerpf(s.identity_coherence, coherence_target, 0.01f * dt);

    /* ── 9. Samskara accumulation ───────────────────────────── */
    int dominant = 0;
    float dom_val = s.rasa[0];
    float prev_dom_val = s.prev_rasa[0];
    for (int i = 1; i < RASA_COUNT; ++i) {
        if (s.rasa[i] > dom_val) { dom_val = s.rasa[i]; dominant = i; }
        if (s.prev_rasa[i] > prev_dom_val) { prev_dom_val = s.prev_rasa[i]; }
    }
    float dom_delta = std::abs(s.rasa[dominant] - s.prev_rasa[dominant]);
    if (dom_delta >= SAMSKARA_THRESHOLD) {
        Samskara& trace = s.samskara_ring[s.samskara_head];
        std::memcpy(trace.rasa_at_moment, s.rasa, sizeof(s.rasa));
        trace.dominant_rasa   = dominant;
        trace.strength        = dom_delta;
        trace.valence         = RASA_VALENCE[dominant];
        trace.buddhi_at_moment= static_cast<int>(s.buddhi_action);
        trace.source_soul_id  = s.last_event_source_id;

        int ctx = 0;
        if (s.bonded_presence < 0.1f && s.isolation > 0.5f) ctx |= CTX_ALONE;
        if (s.bonded_presence > 0.3f)                        ctx |= CTX_BONDED_NEARBY;
        if (s.isolation < 0.3f && s.bonded_presence < 0.1f)  ctx |= CTX_STRANGER_NEAR;
        if (s.void_proximity > 0.5f)                          ctx |= CTX_VOID_PROXIMAL;
        trace.context_hash = ctx;

        s.samskara_head = (s.samskara_head + 1) % MAX_SAMSKARAS;
        if (s.samskara_count < MAX_SAMSKARAS) ++s.samskara_count;
    }
    std::memcpy(s.prev_rasa, s.rasa, sizeof(s.rasa));

    /* ── 10. Vasana recompute (every 100 ticks) ─────────────── */
    if (s.age % 100 == 0) soul_recompute_vasanas(s);

    /* ── 11. Prana */
    float prana_drain = 0.0002f * dt * (rajas + 0.3f);
    float prana_regen = (s.buddhi_action == BUDDHI_REST) ? 0.0005f * dt : 0.0001f * dt;
    s.prana = clamp01(s.prana - prana_drain + prana_regen);
    if (s.prana < 0.2f) s.gunas[GUNA_TAMAS] = std::min(0.9f, s.gunas[GUNA_TAMAS] + 0.001f * dt);

    /* ── 12. Spontaneous rasa events ────────────────────────── */
    s.rasa_event_timer -= dt;
    if (s.rasa_event_timer <= 0.0f) {
        s.rasa_event_timer = g_rng.range(20.0f, 60.0f);
        /* A small spontaneous nudge in the direction of the soul's dominant
         * tendency (vasana-weighted).  Not a scripted event — just noise from
         * the soul's accumulated history. */
        int bias_rasa = dominant;  /* nudge the dominant dimension a tiny bit */
        s.rasa[bias_rasa] = std::min(1.0f, s.rasa[bias_rasa] + g_rng.range(0.0f, 0.03f));
    }

    ++s.age;
}

/* ═══════════════════════════════════════════════════════════════
 * soul_inject_event
 *
 * KEY INVARIANT: no lookup table mapping event_type → fixed deltas.
 * The event profile provides candidate dimensions + direction only.
 * The actual delta is derived entirely from current soul state:
 *
 *   delta = dir × intensity
 *           × governing_guna_value(rasa)   — which guna governs this rasa
 *           × (cur_rasa + 0.1)             — existing resonance amplifies; floor ensures minimal signal
 *           × guna_scale                   — dominant guna colours all incoming signals
 *           × rel_amp                      — bonded/trusted sources land harder
 *           × dvesha_mod                   — pre-existing aversion warps reception
 *
 * Two souls with identical SoulConfig but different state histories
 * WILL produce different outcomes because every factor above depends
 * on accumulated state, not on the event type alone.
 * ═══════════════════════════════════════════════════════════════ */
void soul_inject_event(Soul& s, const char* event_type,
                       float intensity, int source_id,
                       float bond_strength, float trust_to_source,
                       float dvesha_toward_source) {
    if (!s.is_alive) return;

    const EventProfile* prof = find_profile(event_type);
    if (!prof) return;

    float sattva = s.gunas[GUNA_SATTVA];
    float rajas  = s.gunas[GUNA_RAJAS];
    float tamas  = s.gunas[GUNA_TAMAS];

    bool dom_sattva = (sattva >= rajas && sattva >= tamas);
    bool dom_tamas  = (tamas  >  sattva && tamas  >= rajas);

    /* Bonded/trusted source events land harder in both directions */
    float rel_amp = 1.0f + std::min(bond_strength / 5.0f, 0.5f) + trust_to_source * 0.3f;

    for (int ci = 0; ci < prof->n_candidates; ++ci) {
        int idx = prof->candidates[ci].idx;
        int dir = prof->candidates[ci].direction;
        if (idx >= RASA_VISHADA) continue;  /* emergent rasas emerge from tick() only */

        /* 1. Which aspect of consciousness governs this rasa's sensitivity */
        float sensitivity = governing_guna_value(idx, s);

        /* 2. Existing resonance amplifies; a rasa depleted to zero still receives
         *    a minimal signal via the 0.1 floor. */
        float amplifier = s.rasa[idx] + 0.1f;

        /* 3. Dominant guna colours ALL incoming signals.
         *    Sattvic: opens to positive, sheds negative cleanly.
         *    Tamasic: resists both (inertia).
         *    Rajasic: amplifies everything modestly (reactivity). */
        float guna_scale = 1.0f;
        if (dir > 0) {
            if (dom_sattva)     guna_scale = 1.3f;
            else if (dom_tamas) guna_scale = 0.7f;
            else                guna_scale = 1.1f;
        } else {
            if (dom_sattva)     guna_scale = 1.2f;  /* clear mind sheds heavy rasas faster */
            else if (dom_tamas) guna_scale = 0.7f;  /* tamas resists shedding */
            else                guna_scale = 1.0f;
        }

        /* 4. Dvesha modulation: aversion toward source warps how their events land */
        float dvesha_mod = 1.0f;
        if (dir < 0 && dvesha_toward_source > 0.1f)
            dvesha_mod = 1.0f + dvesha_toward_source * 0.4f;  /* hate deepens harm */
        if (dir > 0 && dvesha_toward_source > 0.5f)
            dvesha_mod = 1.0f - dvesha_toward_source * 0.35f; /* distrust blocks positive reception */

        float delta = static_cast<float>(dir)
                    * intensity * sensitivity * amplifier
                    * guna_scale * rel_amp * dvesha_mod;

        s.rasa[idx] = clamp01(s.rasa[idx] + delta);
    }

    /* Rebalance shanta toward equilibrium */
    float total_non_shanta = 0.0f;
    for (int i = 1; i < RASA_COUNT; ++i) total_non_shanta += s.rasa[i];
    s.rasa[RASA_SHANTA] = clampf(1.0f - total_non_shanta * 0.15f, 0.05f, 1.0f);

    s.last_event_source_id = source_id;
    s.disposition_toward_source = soul_disposition(s, bond_strength,
                                                    trust_to_source,
                                                    dvesha_toward_source);
}

/* ═══════════════════════════════════════════════════════════════
 * soul_recompute_vasanas
 * ═══════════════════════════════════════════════════════════════ */
void soul_recompute_vasanas(Soul& s) {
    /* Decay existing vasanas */
    for (int i = 0; i < VASANA_COUNT; ++i)
        s.vasanas[i] *= 0.95f;

    /* Re-aggregate from samskara ring */
    for (int i = 0; i < s.samskara_count; ++i) {
        accumulate_vasana(s.vasanas, s.samskara_ring[i]);
    }

    /* Clamp */
    for (int i = 0; i < VASANA_COUNT; ++i)
        s.vasanas[i] = clampf(s.vasanas[i], -1.0f, 1.0f);
}

/* ═══════════════════════════════════════════════════════════════
 * soul_update_buddhi
 *
 * Derives behavioral inclination from current state.
 * No if/else chains that say "if attacked → flee."
 * Instead: weighted scoring over all candidate actions, where each
 * weight is a function of the current emotional + guna state.
 * ═══════════════════════════════════════════════════════════════ */
void soul_update_buddhi(Soul& s, float nearest_bonded_dist,
                        float nearest_any_dist, int nearest_bonded_id) {
    float scores[9] = {};  /* one per BuddhiAction */

    float bhayanaka = s.rasa[RASA_BHAYANAKA];
    float veera     = s.rasa[RASA_VEERA];
    float shringara = s.rasa[RASA_SHRINGARA];
    float karuna    = s.rasa[RASA_KARUNA];
    float adbhuta   = s.rasa[RASA_ADBHUTA];
    float moha      = s.rasa[RASA_MOHA];
    float sattva    = s.gunas[GUNA_SATTVA];

    /* FLEE: high fear and low courage */
    scores[BUDDHI_FLEE]         = bhayanaka * (1.0f - veera * 0.8f)
                                  + s.rasa[RASA_KRODHA] * 0.1f;

    /* FOLLOW_BOND: strong bonded pull + shringara + proximity */
    float bonded_pull = (nearest_bonded_dist < 50.0f)
        ? shringara * (1.0f - nearest_bonded_dist / 50.0f) * 0.8f : 0.0f;
    scores[BUDDHI_FOLLOW_BOND]  = bonded_pull + s.vasanas[VASANA_FOLLOW_BONDED] * 0.3f;

    /* SEEK_SOUL: social drive from vasana + isolation signal */
    scores[BUDDHI_SEEK_SOUL]    = s.vasanas[VASANA_SEEK_SOULS] * 0.5f
                                  + s.isolation * 0.4f
                                  + karuna * 0.3f;

    /* REST: low prana or high tamas or high moha */
    scores[BUDDHI_REST]         = (1.0f - s.prana) * 0.6f
                                  + s.gunas[GUNA_TAMAS] * 0.4f
                                  + moha * 0.2f
                                  + s.vasanas[VASANA_REST] * 0.3f;

    /* TEACH: sattva-driven, requires others nearby */
    float teach_proximity = (nearest_any_dist < 15.0f) ? 0.5f : 0.0f;
    scores[BUDDHI_TEACH]        = sattva * 0.4f + teach_proximity
                                  + s.vasanas[VASANA_TEACH] * 0.4f;

    /* GUARD_TERRITORY */
    scores[BUDDHI_GUARD_TERRITORY] = s.vasanas[VASANA_GUARD_TERRITORY] * 0.5f
                                     + s.rasa[RASA_MADA] * 0.3f;

    /* HOARD */
    scores[BUDDHI_HOARD]        = s.rasa[RASA_LOBHA] * 0.5f
                                  + s.vasanas[VASANA_HOARD] * 0.4f;

    /* BOND_WITH: directed toward bonded target if shringara high */
    scores[BUDDHI_BOND_WITH]    = shringara * 0.6f
                                  + s.vasanas[VASANA_SEEK_SOULS] * 0.3f;

    /* WANDER: exploration drive */
    scores[BUDDHI_WANDER]       = adbhuta * 0.4f
                                  + s.vasanas[VASANA_EXPLORE] * 0.5f
                                  + 0.1f;  /* small baseline */

    /* Select highest scoring action */
    int best = BUDDHI_WANDER;
    for (int i = 1; i < 9; ++i)
        if (scores[i] > scores[best]) best = i;

    s.buddhi_action = static_cast<BuddhiAction>(best);
    if (best == BUDDHI_FOLLOW_BOND || best == BUDDHI_BOND_WITH)
        s.buddhi_target_id = nearest_bonded_id;
    else
        s.buddhi_target_id = -1;
}

/* ═══════════════════════════════════════════════════════════════
 * soul_disposition
 * ═══════════════════════════════════════════════════════════════ */
float soul_disposition(const Soul& s,
                        float bond_strength, float trust,
                        float dvesha) {
    float warmth   = s.rasa[RASA_SHRINGARA] * 0.3f
                   + trust                   * 0.4f
                   + std::min(bond_strength / 5.0f, 0.3f);
    float hostility = s.rasa[RASA_BHAYANAKA] * 0.3f
                    + dvesha                   * 0.4f
                    + s.rasa[RASA_KRODHA]     * 0.3f;
    return clampf(warmth - hostility, -1.0f, 1.0f);
}

/* ═══════════════════════════════════════════════════════════════
 * soul_get_state
 * ═══════════════════════════════════════════════════════════════ */
SoulState soul_get_state(const Soul& s) {
    SoulState st = {};
    st.id       = s.id;
    st.is_alive = s.is_alive ? 1 : 0;

    std::memcpy(st.rasa,    s.rasa,    sizeof(s.rasa));
    std::memcpy(st.gunas,   s.gunas,   sizeof(s.gunas));
    std::memcpy(st.vasanas, s.vasanas, sizeof(s.vasanas));

    int dom = 0;
    for (int i = 1; i < RASA_COUNT; ++i)
        if (s.rasa[i] > s.rasa[dom]) dom = i;
    st.dominant_rasa = dom;

    st.identity_coherence       = s.identity_coherence;
    st.buddhi_action            = s.buddhi_action;
    st.buddhi_target_id         = s.buddhi_target_id;
    st.prana                    = s.prana;
    st.health                   = s.health;
    st.age                      = s.age;
    st.generation               = s.generation;
    st.karma_rina               = s.karma_rina;
    st.samskara_count           = s.samskara_count;
    st.disposition_toward_source= s.disposition_toward_source;
    return st;
}
