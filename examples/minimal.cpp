/**
 * examples/minimal.cpp
 *
 * Create 10 souls, inject one "attacked" event onto soul 3,
 * tick 100 times, and print soul 3's state before and after.
 *
 * Demonstrates: same event, different outcomes based on state history.
 * Soul 3's attacker is soul 7 — watch disposition_toward_source.
 */

#include "../include/samskriti.h"
#include <cstdio>

static void print_state(const char* label, const SoulState& s) {
    printf("\n%s (soul %d)\n", label, s.id);
    printf("  Dominant rasa idx : %d\n",  s.dominant_rasa);
    printf("  bhayanaka (fear)  : %.3f\n", s.rasa[RASA_BHAYANAKA]);
    printf("  shanta    (peace) : %.3f\n", s.rasa[RASA_SHANTA]);
    printf("  karuna    (comp.) : %.3f\n", s.rasa[RASA_KARUNA]);
    printf("  krodha    (wrath) : %.3f\n", s.rasa[RASA_KRODHA]);
    printf("  Sattva / Rajas / Tamas : %.2f / %.2f / %.2f\n",
           s.gunas[GUNA_SATTVA], s.gunas[GUNA_RAJAS], s.gunas[GUNA_TAMAS]);
    printf("  Buddhi action     : %d\n",   (int)s.buddhi_action);
    printf("  Samskara count    : %d\n",   s.samskara_count);
    printf("  Prana             : %.3f\n", s.prana);
    printf("  Disposition toward attacker (soul 7): %.3f\n",
           s.disposition_toward_source);
}

int main() {
    printf("Samskriti Core — Minimal Example\n");
    printf("version: %s\n\n", samskriti_version());

    /* Initialise with a fixed seed for reproducible output */
    samskriti_init(0xABCD1234ULL);

    /* Spawn 10 souls in a rough cluster so bonds form during ticks */
    SoulConfig cfg = samskriti_default_config();
    for (int i = 0; i < 10; ++i) {
        cfg.position[0] = (float)(i % 4) * 3.0f;
        cfg.position[1] = 0.0f;
        cfg.position[2] = (float)(i / 4) * 3.0f;
        create_soul(cfg);
    }

    /* Tick 30 times to let the world establish some baseline state */
    for (int t = 0; t < 30; ++t) tick(1.0f);

    /* Snapshot BEFORE the event */
    SoulState before = get_state(3);
    print_state("BEFORE attack", before);

    /* Soul 7 attacks soul 3 at intensity 0.7 */
    printf("\n>>> inject_event(soul_3, \"attacked\", 0.7, source=soul_7)\n");
    inject_event(3, "attacked", 0.7f, 7);

    /* Tick 70 more times (100 total) */
    for (int t = 0; t < 70; ++t) tick(1.0f);

    /* Snapshot AFTER */
    SoulState after = get_state(3);
    print_state("AFTER 70 recovery ticks", after);

    /* Also show the bond that formed between 3 and nearby souls */
    printf("\nBond soul_3 ↔ soul_4:\n");
    BondInfo b34 = get_bond(3, 4);
    printf("  bond_strength = %.3f  trust = %.3f  crystallized_floor = %.3f\n",
           b34.bond_strength, b34.trust, b34.crystallized_floor);

    printf("\nLiving souls: %d\n", get_soul_count());

    samskriti_shutdown();
    return 0;
}
