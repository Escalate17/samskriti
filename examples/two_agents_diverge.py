"""The core demonstration.

Two agents start identical. One is helped ten times, the other attacked ten times.
Then both receive the *same* event from the *same* source — and respond differently,
because the engine derives the magnitude of an event from the state receiving it.

Nothing here is scripted. There is no rule that says "if attacked, distrust."

Run: python examples/two_agents_diverge.py
"""
import samskriti as sk


def show(label: str, agent: sk.Agent, other: sk.Agent) -> None:
    s = agent.state
    b = agent.behavior_toward(other)
    top = ", ".join(f"{n} {v:.2f}" for n, v in s.strongest[:3])
    print(f"  {label:<12} {b.action:<18} tone={b.tone:<9} | {top}")


with sk.World(seed=7) as world:
    befriended, betrayed, other = world.spawn(), world.spawn(), world.spawn()
    for a in (befriended, betrayed, other):
        a.move_to(0, 0, 0)

    print("Two identical agents. Different ten-event histories.\n")
    for _ in range(10):
        befriended.experience("helped", 0.7, source=other)
        betrayed.experience("attacked", 0.7, source=other)
        world.step()
    world.step(times=10)

    print("After their histories:")
    show("befriended", befriended, other)
    show("betrayed", betrayed, other)

    # Now the identical event, from the identical source, to both.
    print("\nBoth now receive the same gift, from the same agent:")
    for a in (befriended, betrayed):
        a.experience("received_gift", 0.6, source=other)
    world.step(times=5)

    show("befriended", befriended, other)
    show("betrayed", betrayed, other)

    print("\nSame input. Different outcome. The difference is what they have lived.")
