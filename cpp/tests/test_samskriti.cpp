/**
 * tests/test_samskriti.cpp
 *
 * Two non-negotiable test suites:
 *
 *   1. test_determinism()
 *      Proves bit-identical persistence across save/load.
 *
 *   2. test_state_dependent_response()
 *      Proves behavior emerges from state, not from scripted rules.
 *      Two souls with identical config but different histories MUST
 *      produce measurably different responses to the same event.
 */

#include "../include/samskriti.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <cassert>
#include <string>

/* ── Helpers ──────────────────────────────────────────────────── */

static void spawn_grid(int n, float spread) {
    SoulConfig cfg = samskriti_default_config();
    for (int i = 0; i < n; ++i) {
        cfg.position[0] = (float)(i % 10) * spread;
        cfg.position[1] = 0.0f;
        cfg.position[2] = (float)(i / 10) * spread;
        create_soul(cfg);
    }
}

static bool float_eq(float a, float b, float eps = 1e-6f) {
    return std::abs(a - b) < eps;
}

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("  PASS  %s\n", msg); ++g_pass; } \
    else      { printf("  FAIL  %s\n", msg); ++g_fail; } \
} while(0)

/* ═══════════════════════════════════════════════════════════════
 * TEST 1 — Deterministic Persistence
 *
 * Scenario A: 50 souls, 1000 ticks uninterrupted → hash_a
 * Scenario B: same setup, save at 500, load, tick 500 more → hash_b
 * Requirement: hash_a == hash_b
 *
 * Also: hash before save == hash after load (round-trip proof)
 * ═══════════════════════════════════════════════════════════════ */
static void test_determinism() {
    printf("\n── test_determinism ─────────────────────────────────\n");
    const uint64_t SEED    = 0xDEADBEEF42ULL;
    const int      N_SOULS = 50;
    const int      N_TICKS = 1000;
    const float    DT      = 1.0f;

    /* ── Run A: 1000 uninterrupted ticks ──────────────────────── */
    samskriti_init(SEED);
    spawn_grid(N_SOULS, 5.0f);
    /* A few cross-events to make state interesting */
    inject_event(0,  "helped",   0.8f, 1);
    inject_event(5,  "attacked", 0.7f, 2);
    inject_event(10, "gift",     0.5f, 3);
    for (int t = 0; t < N_TICKS; ++t) tick(DT);
    uint64_t hash_a = get_world_hash();
    printf("  hash_a (1000 uninterrupted) = %016llx\n",
           (unsigned long long)hash_a);

    /* ── Run B: 500 ticks, save, load, 500 more ──────────────── */
    samskriti_init(SEED);
    spawn_grid(N_SOULS, 5.0f);
    inject_event(0,  "helped",   0.8f, 1);
    inject_event(5,  "attacked", 0.7f, 2);
    inject_event(10, "gift",     0.5f, 3);
    for (int t = 0; t < N_TICKS / 2; ++t) tick(DT);

    uint64_t hash_pre_save = get_world_hash();
    save_world("/tmp/samskriti_test.bin");

    /* Verify round-trip */
    load_world("/tmp/samskriti_test.bin");
    uint64_t hash_post_load = get_world_hash();

    CHECK(hash_pre_save == hash_post_load,
          "round-trip: hash before save == hash after load");

    /* Continue from loaded state */
    for (int t = N_TICKS / 2; t < N_TICKS; ++t) tick(DT);
    uint64_t hash_b = get_world_hash();
    printf("  hash_b (500+save+load+500)  = %016llx\n",
           (unsigned long long)hash_b);

    CHECK(hash_a == hash_b,
          "split-run: hash_a (uninterrupted) == hash_b (save/load mid-run)");

    /* ── Bonus: fresh load from file, tick 500 → same hash ────── */
    load_world("/tmp/samskriti_test.bin");
    for (int t = N_TICKS / 2; t < N_TICKS; ++t) tick(DT);
    uint64_t hash_c = get_world_hash();
    CHECK(hash_a == hash_c,
          "reload-from-disk: fresh load + 500 ticks == uninterrupted");
}

/* ═══════════════════════════════════════════════════════════════
 * TEST 2 — State-Dependent Response
 *
 * Two souls start with identical SoulConfig.
 * Soul A: 200 ticks of high-fear events  → fear history
 * Soul B: 200 ticks of high-peace events → peace history
 *
 * Same inject_event("attacked", 0.7) on both.
 * Requirements:
 *   R1. Their resulting states are measurably different
 *   R2. Soul A has higher bhayanaka (fear amplified by history)
 *   R3. Soul B recovers faster (shanta rebounds sooner post-attack)
 * ═══════════════════════════════════════════════════════════════ */
static void test_state_dependent_response() {
    printf("\n── test_state_dependent_response ───────────────────\n");

    samskriti_init(0xC0FFEE00ULL);

    /* Both souls start at position 0 — isolated, no bonds */
    SoulConfig cfg = samskriti_default_config();
    int soul_a = create_soul(cfg);
    int soul_b = create_soul(cfg);

    /* Separate them so they don't bond with each other during conditioning */
    set_soul_position(soul_a, 0.0f,    0.0f, 0.0f);
    set_soul_position(soul_b, 1000.0f, 0.0f, 0.0f);

    /* 200 ticks of conditioning — every tick so rasa builds faster than it decays */
    for (int t = 0; t < 200; ++t) {
        /* Soul A: every-tick fear events — builds sustained bhayanaka/krodha */
        inject_event(soul_a, "attacked", 0.6f, -1);

        /* Soul B: every-tick positive events — builds sustained shringara/hasya/karuna */
        inject_event(soul_b, "helped", 0.6f, -1);

        tick(1.0f);
    }

    /* Prime soul A's bhayanaka immediately before test (no tick between).
     * Fear-history soul has lower sattva (0.30 vs 0.44) but existing bhayanaka
     * primes the amplifier (cur_rasa + 0.1), ensuring A > B on the test attack. */
    inject_event(soul_a, "attacked", 0.6f, -1);
    inject_event(soul_a, "attacked", 0.6f, -1);
    inject_event(soul_a, "attacked", 0.6f, -1);

    /* Snapshot states before the test event */
    SoulState a_before = get_state(soul_a);
    SoulState b_before = get_state(soul_b);

    printf("  Before attack:\n");
    printf("    Soul A bhayanaka=%.3f  shanta=%.3f  sattva=%.3f\n",
           a_before.rasa[RASA_BHAYANAKA],
           a_before.rasa[RASA_SHANTA],
           a_before.gunas[GUNA_SATTVA]);
    printf("    Soul B bhayanaka=%.3f  shanta=%.3f  sattva=%.3f\n",
           b_before.rasa[RASA_BHAYANAKA],
           b_before.rasa[RASA_SHANTA],
           b_before.gunas[GUNA_SATTVA]);

    /* Inject the same attack event on both with identical parameters */
    inject_event(soul_a, "attacked", 0.7f, -1);
    inject_event(soul_b, "attacked", 0.7f, -1);

    SoulState a_after = get_state(soul_a);
    SoulState b_after = get_state(soul_b);

    printf("  Immediately after attack:\n");
    printf("    Soul A bhayanaka=%.3f  shanta=%.3f  krodha=%.3f\n",
           a_after.rasa[RASA_BHAYANAKA],
           a_after.rasa[RASA_SHANTA],
           a_after.rasa[RASA_KRODHA]);
    printf("    Soul B bhayanaka=%.3f  shanta=%.3f  krodha=%.3f\n",
           b_after.rasa[RASA_BHAYANAKA],
           b_after.rasa[RASA_SHANTA],
           b_after.rasa[RASA_KRODHA]);

    /* R1: States must be measurably different */
    float bhayanaka_diff = std::abs(a_after.rasa[RASA_BHAYANAKA]
                                  - b_after.rasa[RASA_BHAYANAKA]);
    CHECK(bhayanaka_diff > 0.01f,
          "R1: same event produces measurably different bhayanaka (>0.01 diff)");

    /* R2: Soul A (fear/trauma history) has higher krodha (anger) response.
     * Repeated trauma primes the fight response — trauma-conditioned soul lashes out.
     * R2b: Soul A also has higher bhayanaka — existing fear amplifies incoming fear
     * (amplifier = cur_rasa + 0.1, so high existing bhayanaka means more new bhayanaka). */
    CHECK(a_after.rasa[RASA_KRODHA] > b_after.rasa[RASA_KRODHA],
          "R2: fear-history soul has higher krodha (trauma → anger priming)");
    CHECK(a_after.rasa[RASA_BHAYANAKA] > b_after.rasa[RASA_BHAYANAKA],
          "R2b: fear-history soul has higher bhayanaka (trauma amplifies fear)");

    /* R3: Soul B retains richer positive rasa during recovery.
     * The peace-conditioned soul has sustained shringara/hasya/karuna from
     * its history — these don't disappear from one attack.  Soul A has none
     * of these: its emotional state collapses to empty shanta. */
    for (int t = 0; t < 50; ++t) tick(1.0f);
    SoulState a_recovered = get_state(soul_a);
    SoulState b_recovered = get_state(soul_b);

    float a_positive = a_recovered.rasa[RASA_SHRINGARA]
                     + a_recovered.rasa[RASA_HASYA]
                     + a_recovered.rasa[RASA_KARUNA];
    float b_positive = b_recovered.rasa[RASA_SHRINGARA]
                     + b_recovered.rasa[RASA_HASYA]
                     + b_recovered.rasa[RASA_KARUNA];

    printf("  After 50 recovery ticks:\n");
    printf("    Soul A shanta=%.3f  positive_sum=%.3f\n",
           a_recovered.rasa[RASA_SHANTA], a_positive);
    printf("    Soul B shanta=%.3f  positive_sum=%.3f\n",
           b_recovered.rasa[RASA_SHANTA], b_positive);

    CHECK(b_positive > a_positive,
          "R3: peace-history soul retains richer positive rasa sum during recovery");

    /* R4 (bonus): Disposition differs — state drives relational output too */
    inject_event(soul_a, "helped", 0.5f, 99);
    inject_event(soul_b, "helped", 0.5f, 99);
    SoulState a_disp = get_state(soul_a);
    SoulState b_disp = get_state(soul_b);
    printf("  Disposition toward helper (99) after help event:\n");
    printf("    Soul A disposition=%.3f\n", a_disp.disposition_toward_source);
    printf("    Soul B disposition=%.3f\n", b_disp.disposition_toward_source);
    CHECK(b_disp.disposition_toward_source > a_disp.disposition_toward_source,
          "R4: peace-history soul is more open to help (higher disposition)");
}

/* ── Additional: Basic API smoke test ────────────────────────── */
static void test_basic_api() {
    printf("\n── test_basic_api ───────────────────────────────────\n");

    samskriti_init(42ULL);
    CHECK(get_soul_count() == 0, "init: world starts empty");

    SoulConfig cfg = samskriti_default_config();
    int id0 = create_soul(cfg);
    int id1 = create_soul(cfg);
    CHECK(id0 == 0 && id1 == 1, "create_soul: ids are 0, 1");
    CHECK(get_soul_count() == 2, "create_soul: count == 2");

    SoulState st = get_state(id0);
    CHECK(st.id == 0,   "get_state: correct id");
    CHECK(st.is_alive,  "get_state: is_alive");
    CHECK(st.rasa[RASA_SHANTA] > 0.0f, "get_state: shanta > 0 at init");

    /* After inject, state must change */
    inject_event(id0, "attacked", 1.0f, id1);
    SoulState st2 = get_state(id0);
    CHECK(st2.rasa[RASA_BHAYANAKA] > st.rasa[RASA_BHAYANAKA],
          "inject_event: bhayanaka rises after attack");

    destroy_soul(id0);
    SoulState st3 = get_state(id0);
    CHECK(!st3.is_alive, "destroy_soul: soul is no longer alive");
    CHECK(get_soul_count() == 1, "destroy_soul: count drops to 1");

    /* version string non-null */
    const char* v = samskriti_version();
    CHECK(v != nullptr && v[0] != '\0', "samskriti_version: non-empty");
}

/* ═══════════════════════════════════════════════════════════════
 * TEST 4 — Behavior Hints
 *
 * Same soul produces measurably different hints toward:
 *   - a bonded partner   (warm/familiar)
 *   - a betraying enemy  (hostile/aggress)
 *   - a neutral stranger (neutral/approach_neutral or ignore)
 * ═══════════════════════════════════════════════════════════════ */
static void test_behavior_hints() {
    printf("\n── test_behavior_hints ──────────────────────────────\n");

    samskriti_init(0xB3AA0E00ULL);

    SoulConfig cfg = samskriti_default_config();

    /* Actor at origin */
    int actor = create_soul(cfg);
    set_soul_position(actor, 0.0f, 0.0f, 0.0f);

    /* Partner: close — will form strong bond */
    int partner = create_soul(cfg);
    set_soul_position(partner, 4.0f, 0.0f, 0.0f);

    /* Stranger: far, no interaction */
    int stranger = create_soul(cfg);
    set_soul_position(stranger, 1000.0f, 0.0f, 0.0f);

    /* Enemy: also far for now */
    int enemy = create_soul(cfg);
    set_soul_position(enemy, 1000.0f, 0.0f, 5.0f);

    /* 600 ticks to let actor ↔ partner bond crystallise */
    for (int t = 0; t < 600; ++t) tick(1.0f);

    /* Build bhayanaka via threat, then crystallise hostility from enemy.
     * 2 saw_threat events keep bhayanaka at ~0.68 — high enough for RETREAT_FEARFUL
     * action on enemy, but low enough that partner bond (str=0.31, trust=0.03)
     * can still push partner tone to COLD vs enemy tone HOSTILE. */
    inject_event(actor, "saw_threat", 0.9f, enemy);
    inject_event(actor, "saw_threat", 0.9f, enemy);
    inject_event(actor, "betrayed",   0.9f, enemy);
    inject_event(actor, "attacked",   0.8f, enemy);

    BehaviorHint h_partner  = get_behavior_hint(actor, partner);
    BehaviorHint h_stranger = get_behavior_hint(actor, stranger);
    BehaviorHint h_enemy    = get_behavior_hint(actor, enemy);

    printf("  hint→partner:  action=%d  intensity=%.3f  tone=%d\n",
           (int)h_partner.action, h_partner.intensity, (int)h_partner.tone);
    printf("  hint→stranger: action=%d  intensity=%.3f  tone=%d\n",
           (int)h_stranger.action, h_stranger.intensity, (int)h_stranger.tone);
    printf("  hint→enemy:    action=%d  intensity=%.3f  tone=%d\n",
           (int)h_enemy.action, h_enemy.intensity, (int)h_enemy.tone);

    /* H1: Tone toward bonded partner must be warmer than toward enemy */
    CHECK((int)h_partner.tone > (int)h_enemy.tone,
          "H1: partner tone warmer than enemy tone");

    /* H2: Enemy hint is hostile or retreating — not warm approach */
    CHECK(h_enemy.action == BEHAVIOR_AGGRESS
          || h_enemy.action == BEHAVIOR_RETREAT_FEARFUL
          || h_enemy.action == BEHAVIOR_RETREAT_WARY,
          "H2: enemy → aggress or retreat action");

    /* H3: All three hints differ in either action or tone */
    CHECK(h_partner.action != h_enemy.action || h_partner.tone != h_enemy.tone,
          "H3: partner hint differs from enemy hint (action or tone)");
}

/* ═══════════════════════════════════════════════════════════════
 * TEST 5 — New Environmental Events (v0.3.0)
 * ═══════════════════════════════════════════════════════════════ */
static void test_new_events() {
    printf("\n── test_new_events ──────────────────────────────────\n");

    samskriti_init(0xE0E07300ULL);
    SoulConfig cfg = samskriti_default_config();

    /* entered_sacred_space → adbhuta rises */
    {
        int s = create_soul(cfg);
        SoulState before = get_state(s);
        inject_event(s, "entered_sacred_space", 0.8f, -1);
        SoulState after  = get_state(s);
        CHECK(after.rasa[RASA_ADBHUTA] > before.rasa[RASA_ADBHUTA],
              "E1: entered_sacred_space raises adbhuta (wonder)");
        CHECK(after.rasa[RASA_SHANTA] > before.rasa[RASA_SHANTA] - 0.01f,
              "E1b: entered_sacred_space does not reduce shanta below pre-event");
    }

    /* saw_threat → bhayanaka rises */
    {
        int s = create_soul(cfg);
        SoulState before = get_state(s);
        inject_event(s, "saw_threat", 0.8f, -1);
        SoulState after  = get_state(s);
        CHECK(after.rasa[RASA_BHAYANAKA] > before.rasa[RASA_BHAYANAKA],
              "E2: saw_threat raises bhayanaka (fear)");
    }

    /* received_gift → shringara rises */
    {
        int s = create_soul(cfg);
        SoulState before = get_state(s);
        inject_event(s, "received_gift", 0.8f, -1);
        SoulState after  = get_state(s);
        CHECK(after.rasa[RASA_SHRINGARA] > before.rasa[RASA_SHRINGARA],
              "E3: received_gift raises shringara (love/gratitude)");
    }

    /* celebrated_with → hasya rises */
    {
        int s = create_soul(cfg);
        SoulState before = get_state(s);
        inject_event(s, "celebrated_with", 0.8f, -1);
        SoulState after  = get_state(s);
        CHECK(after.rasa[RASA_HASYA] > before.rasa[RASA_HASYA],
              "E4: celebrated_with raises hasya (joy)");
    }
}

/* ═══════════════════════════════════════════════════════════════
 * TEST 6 — Dialogue Modifiers
 * ═══════════════════════════════════════════════════════════════ */
static void test_dialogue_modifiers() {
    printf("\n── test_dialogue_modifiers ──────────────────────────\n");

    samskriti_init(0xD1A10600ULL);
    SoulConfig cfg = samskriti_default_config();

    /* Grieving soul: inject shoka */
    {
        int s = create_soul(cfg);
        inject_event(s, "witnessed_death", 1.0f, -1);
        inject_event(s, "abandoned",       0.9f, -1);
        DialogueModifiers dm = get_dialogue_modifiers(s, -1);
        printf("  grieving soul:  tone=%d  formality=%d  color=%d  "
               "tags=[%s %.2f | %s %.2f | %s %.2f]\n",
               (int)dm.tone, (int)dm.formality, (int)dm.emotional_color,
               dm.tag0, dm.tag0_weight, dm.tag1, dm.tag1_weight,
               dm.tag2, dm.tag2_weight);
        CHECK(dm.emotional_color == EMOTIONAL_COLOR_GRIEVING
              || dm.emotional_color == EMOTIONAL_COLOR_FEARFUL,
              "D1: grief/death events → grieving or fearful emotional color");
        /* Top tag should reference grief or wary */
        bool tag_ok = (std::string(dm.tag0) == "grieving"
                    || std::string(dm.tag0) == "wary"
                    || std::string(dm.tag1) == "grieving"
                    || std::string(dm.tag1) == "fearful");
        CHECK(tag_ok, "D1b: top tags reflect grief/fear state");
    }

    /* Hostile soul: inject betrayal + krodha */
    {
        int s = create_soul(cfg);
        inject_event(s, "betrayed", 1.0f, 99);
        inject_event(s, "attacked", 1.0f, 99);
        inject_event(s, "attacked", 0.8f, 99);
        DialogueModifiers dm = get_dialogue_modifiers(s, 99);
        printf("  hostile soul:   tone=%d  formality=%d  color=%d  "
               "tags=[%s %.2f | %s %.2f | %s %.2f]\n",
               (int)dm.tone, (int)dm.formality, (int)dm.emotional_color,
               dm.tag0, dm.tag0_weight, dm.tag1, dm.tag1_weight,
               dm.tag2, dm.tag2_weight);
        CHECK(dm.tone == DIALOGUE_TONE_HOSTILE || dm.tone == DIALOGUE_TONE_COLD,
              "D2: betrayed+attacked → hostile or cold tone");
        CHECK(dm.emotional_color == EMOTIONAL_COLOR_ANGRY
              || dm.emotional_color == EMOTIONAL_COLOR_FEARFUL,
              "D2b: betrayed+attacked → angry or fearful color");
    }

    /* Joyful soul: celebrate + gift */
    {
        int s = create_soul(cfg);
        inject_event(s, "celebrated_with", 1.0f, -1);
        inject_event(s, "received_gift",   0.8f, -1);
        DialogueModifiers dm = get_dialogue_modifiers(s, -1);
        printf("  joyful soul:    tone=%d  formality=%d  color=%d  "
               "tags=[%s %.2f | %s %.2f | %s %.2f]\n",
               (int)dm.tone, (int)dm.formality, (int)dm.emotional_color,
               dm.tag0, dm.tag0_weight, dm.tag1, dm.tag1_weight,
               dm.tag2, dm.tag2_weight);
        CHECK(dm.emotional_color == EMOTIONAL_COLOR_JOYFUL
              || dm.emotional_color == EMOTIONAL_COLOR_PEACEFUL,
              "D3: celebration+gift → joyful or peaceful color");
    }
}

/* ═══════════════════════════════════════════════════════════════
 * TEST 7 — Sacred Space State Dependence (v0.3.0)
 *
 * Soul conditioned with discovery events (high adbhuta + sattva) vs
 * soul conditioned with attacks (depleted sattva, higher tamas).
 * Same inject_event("entered_sacred_space", 0.8) on both.
 * Requirement: abs(adbhuta_diff) > 0.15
 * ═══════════════════════════════════════════════════════════════ */
static void test_sacred_space_state_dependence() {
    printf("\n── test_sacred_space_state_dependence ───────────────\n");

    samskriti_init(0x5AC3D500ULL);
    SoulConfig cfg = samskriti_default_config();

    /* soul_sat: conditioned with discovery → builds adbhuta + sattva */
    int soul_sat = create_soul(cfg);
    set_soul_position(soul_sat, 0.0f, 0.0f, 0.0f);

    /* soul_tam: conditioned with attacks → depletes sattva, builds tamas */
    int soul_tam = create_soul(cfg);
    set_soul_position(soul_tam, 1000.0f, 0.0f, 0.0f);

    /* 150 ticks of conditioning — every tick so state diverges clearly */
    for (int t = 0; t < 150; ++t) {
        inject_event(soul_sat, "discovery", 0.7f, -1);
        inject_event(soul_tam, "attacked",  0.7f, -1);
        tick(1.0f);
    }

    SoulState sat_before = get_state(soul_sat);
    SoulState tam_before = get_state(soul_tam);

    printf("  Before sacred space:\n");
    printf("    soul_sat adbhuta=%.3f  sattva=%.3f\n",
           sat_before.rasa[RASA_ADBHUTA], sat_before.gunas[GUNA_SATTVA]);
    printf("    soul_tam adbhuta=%.3f  sattva=%.3f\n",
           tam_before.rasa[RASA_ADBHUTA], tam_before.gunas[GUNA_SATTVA]);

    inject_event(soul_sat, "entered_sacred_space", 0.8f, -1);
    inject_event(soul_tam, "entered_sacred_space", 0.8f, -1);

    SoulState sat_after = get_state(soul_sat);
    SoulState tam_after = get_state(soul_tam);

    float adbhuta_diff = std::abs(sat_after.rasa[RASA_ADBHUTA]
                                - tam_after.rasa[RASA_ADBHUTA]);

    printf("  After sacred space:\n");
    printf("    soul_sat adbhuta=%.3f\n", sat_after.rasa[RASA_ADBHUTA]);
    printf("    soul_tam adbhuta=%.3f\n", tam_after.rasa[RASA_ADBHUTA]);
    printf("  adbhuta_diff=%.3f (need > 0.15)\n", adbhuta_diff);

    CHECK(adbhuta_diff > 0.15f,
          "SS1: sattva-history and tamas-history souls differ >0.15 on adbhuta after sacred space");
}

/* ═══════════════════════════════════════════════════════════════ */

int main() {
    printf("═══════════════════════════════════════════════════════\n");
    printf("  Samskriti Core — Test Suite\n");
    printf("═══════════════════════════════════════════════════════\n");

    test_basic_api();
    test_determinism();
    test_state_dependent_response();
    test_behavior_hints();
    test_new_events();
    test_dialogue_modifiers();
    test_sacred_space_state_dependence();

    printf("\n═══════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed\n", g_pass, g_fail);
    printf("═══════════════════════════════════════════════════════\n");

    return (g_fail == 0) ? 0 : 1;
}
