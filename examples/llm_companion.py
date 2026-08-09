"""Wiring the engine to a language model.

The engine holds who the character *is*. The model only says it. This example shows the
handoff and deliberately stops there — classifying user input into an event, and turning
state into prose, are your layers, and they are where your product's character lives.

No API key needed; the "LLM call" is printed rather than sent.

Run: python examples/llm_companion.py
"""
import samskriti as sk

# Your perception layer decides what an incoming message *was*. A keyword check, a
# classifier, a small model — the engine does not care, it only needs an event kind
# and an intensity.
def classify(message: str) -> tuple[str, float] | None:
    m = message.lower()
    if any(w in m for w in ("thank", "appreciate", "helped")):
        return "helped", 0.7
    if any(w in m for w in ("useless", "stupid", "hate", "pathetic")):
        return "attacked", 0.7
    if any(w in m for w in ("sorry", "apologise", "apologize")):
        return "reunion", 0.5
    return None


def build_prompt(agent: sk.Agent, user: sk.Agent) -> str:
    """Turn computed state into instructions. Keep this thin — it is a translation,
    not a personality. The personality already happened, in the engine."""
    voice = agent.voice_toward(user)
    state = agent.state
    feeling = ", ".join(f"{n}" for n, _ in state.strongest[:2]) or "settled"
    return (
        f"You are speaking to someone you feel {voice.tone} toward.\n"
        f"Your manner right now is {voice.formality}; the colour of your mood is "
        f"{voice.color}.\n"
        f"What is most alive in you: {feeling}.\n"
        f"Do not describe these feelings. Speak from inside them."
    )


with sk.World(seed=3) as world:
    companion, user = world.spawn(), world.spawn()
    companion.move_to(0, 0, 0)
    user.move_to(1, 0, 0)

    # An arc: warmth, then a rupture, then an attempt at repair. One tick per turn —
    # state accumulates at roughly the rate experience arrives (see robot_companion.py
    # for why that ratio matters).
    conversation = [
        "thanks for helping me with that",
        "seriously, you've been a huge help",
        "appreciate you sticking with it",
        "honestly you're useless",
        "this is stupid, you're pathetic",
        "you never actually help",
        "okay that was harsh, sorry",
        "sorry, really. that wasn't fair of me",
    ]

    for message in conversation:
        event = classify(message)
        if event:
            companion.experience(event[0], event[1], source=user)
        world.step()
        voice = companion.voice_toward(user)
        felt = ", ".join(n for n, _ in companion.state.strongest[:2]) or "settled"
        print(f'USER: {message}')
        print(f'  -> feels {voice.tone}/{voice.color}, alive in it: {felt}')

    print("\n--- prompt handed to the model on the next turn ---")
    print(build_prompt(companion, user))

    # Persist between sessions: save the world, reload it later, the character continues.
    world.save("/tmp/companion_state.bin")
    print("\n[state saved — reload it next session and the relationship is intact]")
