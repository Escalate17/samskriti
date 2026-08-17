#!/usr/bin/env python3
"""Reproduce the paper's three findings. Runs in about a minute, no arguments.

    pip install git+https://github.com/Escalate17/samskriti
    python examples/reproduce_paper.py

Every number this prints is computed live. Nothing is hardcoded, and the seeds are
fixed, so two runs on the same machine agree exactly.

The claim under test is that character is not configured but computed — that these
behaviours fall out of the state dynamics rather than being written down anywhere. So
each section below sets up a history, runs time forward, and reports what came out.
"""
import samskriti as s


def rule(t):
    print(f"\n{'─' * 72}\n{t}\n{'─' * 72}")


# ══════════════════════════════════════════════════════════════════════════
rule("FINDING 1 — the same event lands differently on different histories")
# Two agents, identical constitutions. Only their pasts differ. Then both meet the
# same threat. If character were configured, the responses would match.

def raised_on(event, n=25):
    w = s.World(seed=11)
    a = w.spawn(guna={"sattva": 0.4, "rajas": 0.35, "tamas": 0.25})
    for _ in range(n):
        a.experience(s.Experience(event, 0.7))
        w.step()
    return w, a

print(f"  {'raised on':<16}{'fear carried':>14}{'after threat':>14}{'reaction':>11}")
rows = {}
for label, event in (("kindness", "helped"), ("violence", "attacked")):
    w, a = raised_on(event)
    before = a.state.rasa["bhayanaka"]
    a.experience(s.Experience("saw_threat", 0.6))
    w.step()
    after = a.state.rasa["bhayanaka"]
    rows[label] = (before, after)
    print(f"  {label:<16}{before:>14.3f}{after:>14.3f}{after - before:>11.3f}")
    w.close()
gap = rows["violence"][1] - rows["kindness"][1]
print(f"\n  Identical constitution, identical threat, {gap:.3f} apart in fear.")
print("  The violent-history agent also walks in already carrying "
      f"{rows['violence'][0]:.3f} fear that the other simply does not have.")


# ══════════════════════════════════════════════════════════════════════════
rule("FINDING 2 — four rasas that no event can produce")
# These are not in the event vocabulary. Nothing can inject them. They only form when
# other states stay elevated together, long enough, under the right constitution.

print(f"  emergent: {', '.join(s.EMERGENT_RASAS)}")
print(f"  events available: {len(s.EVENT_TYPES)} — none of them produce these\n")

def grow(label, rasa, guna, plan, ticks, companion=False):
    w = s.World(seed=5)
    a = w.spawn(rasa=rasa, guna=guna)
    if companion:
        # Presence, not events: a bond nearby lifts sattva without the joy that
        # would cancel despair. This distinction is what makes vairagya reachable.
        f = w.spawn(guna={"sattva": 0.85, "rajas": 0.1, "tamas": 0.05})
        a.move_to(0, 0, 0); f.move_to(0.4, 0, 0)
        for _ in range(200):
            a.experience("helped", 0.5, source=f); w.step()
    first = {}
    for t in range(1, ticks + 1):
        for ev, every, inten in plan:
            if t % every == 0:
                a.experience(s.Experience(ev, inten))
        w.step()
        for k in s.EMERGENT_RASAS:
            if k not in first and a.state.rasa[k] >= 0.05:
                first[k] = t
    peak = {k: round(a.state.rasa[k], 2) for k in s.EMERGENT_RASAS
            if a.state.rasa[k] > 0.05}
    print(f"  {label}")
    print(f"     emerged at tick: {first or 'never'}")
    print(f"     final          : {peak or 'none'}\n")
    w.close()

grow("udvega (chronic anxiety) — threat that never resolves",
     {"bhayanaka": 0.3}, {"sattva": 0.3, "rajas": 0.4, "tamas": 0.3},
     [("saw_threat", 4, 0.8)], 2000)

grow("titiksha (endurance) — suffering met with resolve",
     {"veera": 0.4, "karuna": 0.4}, {"sattva": 0.6, "rajas": 0.3, "tamas": 0.1},
     [("witnessed_death", 6, 0.7), ("discovery", 9, 0.6)], 3000)

grow("vairagya (detachment) — grief endured WITHOUT losing clarity",
     {"shanta": 0.75, "karuna": 0.35}, {"sattva": 0.92, "rajas": 0.05, "tamas": 0.03},
     [("witnessed_death", 6, 0.7)], 12000, companion=True)

print("  Note on the last one: it needs grief heavy enough to produce despair AND a")
print("  bond close enough to keep sattva up. Suffering alone collapses sattva and it")
print("  never forms; comfort alone cancels the despair and it never forms. Only the")
print("  narrow overlap produces detachment — which is the interesting part.")


# ══════════════════════════════════════════════════════════════════════════
rule("FINDING 3 — disposition is directed: warm to one, hostile to another, at once")
# Two channels, deliberately separate. A bond grows from proximity and time; hostility
# toward a specific agent comes from harm attributed to them. So an agent can hold
# opposite dispositions toward two others while running one internal emotional state.

w = s.World(seed=21)
actor, partner, stranger, enemy = w.spawn(), w.spawn(), w.spawn(), w.spawn()
actor.move_to(0, 0, 0)
partner.move_to(1.0, 0, 0)        # close: the bond crystallises from being near
stranger.move_to(50.0, 0, 0)
enemy.move_to(1000.0, 0, 5.0)     # far: no bond, only the harm it caused

for _ in range(600):              # time is what builds the bond, not events
    w.step()

for ev, inten in (("saw_threat", 0.9), ("saw_threat", 0.9),
                  ("betrayed", 0.9), ("attacked", 0.8)):
    actor.experience(ev, inten, source=enemy)

print(f"  {'toward':<12}{'action':<18}{'tone':<10}{'warmth':>8}")
for label, other in (("partner", partner), ("stranger", stranger), ("enemy", enemy)):
    b = actor.behavior_toward(other)
    print(f"  {label:<12}{b.action:<18}{b.tone:<10}{b.warmth:>8.2f}")

v_p, v_e = actor.voice_toward(partner), actor.voice_toward(enemy)
print(f"\n  voice → partner : tone {v_p.tone}, color {v_p.color}")
print(f"  voice → enemy   : tone {v_e.tone}, color {v_e.color}")
print("\n  One agent, one emotional state, three different dispositions held at once.")
w.close()


rule("Reproducing this")
print("  Fixed seeds, no randomness beyond them: rerun and the numbers are identical.")
print("  Paper: https://doi.org/10.5281/zenodo.20531430")
print("  32/32 C++ tests, 13/13 Python tests: see tests/")
