"""A robot shaped by how it is treated — and the one tuning fact you need first.

For embedded use the appeal is size: the whole character is 188 bytes of state plus a
small library. No GPU, no network, no model. It runs on the robot and survives a reboot.

READ THIS BEFORE BUILDING ON IT
-------------------------------
Rasas decay every tick. Whether a character *accumulates* a disposition or washes back to
equilibrium depends on how often experience arrives relative to how fast you advance time.
At the default decay rates:

    ~1 tick between events   → state accumulates; disposition forms and holds
    ~2 ticks between events  → state accumulates more slowly
    4+ ticks between events  → decay dominates; everything returns to shanta (peace)

That is a deliberate property, not a bug: an agent left alone becomes calm. But it means
`tick()` is your modelling dial, not a wall clock. If you want a robot that carries a mood
across a sparse day, either tick less often than real time, or slow the decay rates in
cpp/src/soul.cpp (STHAYI_DECAY). Do not assume one tick equals one second.

The scenario below therefore models an *interactive* robot — many exchanges, time advanced
between them — rather than one idling for months.

Run: python examples/robot_companion.py
"""
import samskriti as sk

EXCHANGES = 60

HOUSEHOLDS = {
    "warm":  [("celebrated_with", 0.6), ("helped", 0.6)],
    "harsh": [("attacked", 0.6), ("betrayed", 0.5)],
}


def live(label: str, routine: list[tuple[str, float]]) -> None:
    with sk.World(seed=17) as world:
        robot, person = world.spawn(), world.spawn()
        robot.move_to(0, 0, 0)
        person.move_to(1, 0, 0)

        for _ in range(EXCHANGES):
            for kind, intensity in routine:
                robot.experience(kind, intensity, source=person)
            world.step()          # one tick per exchange — see the note above

        s = robot.state
        b = robot.behavior_toward(person)
        v = robot.voice_toward(person)
        top = ", ".join(f"{n} {val:.2f}" for n, val in s.strongest[:3])

        print(f"\n{label} household — after {EXCHANGES} exchanges")
        print(f"  disposition  : {b.action} ({b.tone})")
        print(f"  manner       : {v.formality}, {v.color}")
        print(f"  felt state   : {top}")
        print(f"  constitution : " + ", ".join(f"{k} {val:.2f}" for k, val in s.guna.items()))
        print(f"  impressions  : {s.samskara_count}")


print(f"Identical robots. {EXCHANGES} exchanges each. Different households.")
for label, routine in HOUSEHOLDS.items():
    live(label, routine)

print("\nThe hardware never changed. Only what happened to it did.")
