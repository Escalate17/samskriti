#!/usr/bin/env python3
"""Talk to a character and watch its state change, with the arithmetic shown.

    pip install git+https://github.com/Escalate17/samskriti
    samskriti

Type anything. After each line you see four things:

    READ     what the engine understood the message to be
    MOVED    which feelings changed, by exactly how much
    STATE    where the character is now
    TOWARD   how it currently feels about YOU specifically

Nothing here is scripted. There is no dialogue tree and no list of responses. The
character has no opinion of you at the start and acquires one only from what you type.

WHY THE CLASSIFIER IS DELIBERATELY STUPID
Turning a sentence into an event is a separate problem from what the event does to a
character, and only the second one is this engine's claim. So the classifier below is
forty lines of keyword matching that you can read in a minute and disagree with out
loud — which is the point. In production that layer would be a language model, and it
would be the one part of the pipeline you could NOT inspect. Everything after it stays
arithmetic you can check by hand.

ABOUT THE WEIGHT
At its reference gain the engine is built for lifetimes — thousands of moments — so one
sentence moves a feeling by about 0.03, which you would not notice in a ten-message
conversation. A typed line here is therefore applied WEIGHT times, because a sentence in
a conversation is not one moment: it is the memorable part of an exchange that contained
many. The multiplier is printed on every turn and you can change it below or with
--weight. Nothing is hidden; set it to 1 to see the raw reference behaviour.

Commands:  :why <feeling>   full history of one feeling
           :state           all non-zero feelings
           :quit
"""
import argparse

import samskriti as s

# How many engine moments one typed line represents. The Luau port of this engine landed
# on 6.0 for the same reason: a game NPC gets a dozen interactions, not ten thousand.
WEIGHT = 6

# ── the deliberately-inspectable classifier ────────────────────────────────
# word -> (event, how hard it lands). First match wins; longest phrases first.
CUES = [
    (("piece of shit", "fuck you", "i hate you", "worthless"), "attacked", 0.95),
    (("betrayed", "lied to me", "you lied", "backstabbed"),    "betrayed", 0.90),
    (("useless", "stupid", "pathetic", "idiot", "shut up"),    "attacked", 0.75),
    (("hate", "awful", "terrible", "disappointing"),           "attacked", 0.55),
    (("left me", "abandoned", "alone", "nobody"),              "abandoned", 0.70),
    (("died", "death", "passed away", "funeral"),        "witnessed_death", 0.80),
    (("scared", "afraid", "threat", "danger", "unsafe"),     "saw_threat", 0.65),
    (("thank", "grateful", "appreciate", "you helped"),          "helped", 0.75),
    (("love you", "adore", "means everything"),           "received_gift", 0.85),
    (("gift", "present", "brought you", "for you"),                "gift", 0.70),
    (("congrat", "celebrate", "we did it", "amazing"),  "celebrated_with", 0.70),
    (("missed you", "you're back", "good to see"),              "reunion", 0.75),
    (("discovered", "figured out", "found out", "learned"),   "discovery", 0.60),
    (("beautiful", "peaceful", "sacred", "wonder"), "entered_sacred_space", 0.60),
]


def classify(text: str):
    low = text.lower()
    for words, event, strength in CUES:
        for w in words:
            if w in low:
                # SHOUTING and !!! make it land harder — the only nuance in here.
                emph = min(0.25, low.count("!") * 0.08)
                if text.isupper() and len(text) > 6:
                    emph += 0.15
                return event, min(1.0, strength + emph), w
    return None, 0.0, None


def bar(v: float, width: int = 18) -> str:
    n = max(0, min(width, round(v * width)))
    return "█" * n + "·" * (width - n)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--weight", type=int, default=WEIGHT,
                    help="engine moments per typed line (1 = raw reference gain)")
    args = ap.parse_args()
    weight = max(1, args.weight)

    world = s.World(seed=7)
    # A middling constitution — not a saint, not a brute. Change these two numbers and
    # the same conversation produces a different person; that IS the demo.
    them = world.spawn(rasa={"shanta": 0.7}, guna={"sattva": 0.45, "rajas": 0.35,
                                                   "tamas": 0.20})
    you = world.spawn()
    them.move_to(0, 0, 0); you.move_to(1.0, 0, 0)

    history: dict[str, list] = {}
    turn = 0

    print(__doc__.split("Commands:")[0].rstrip())
    print("Commands:  :why <feeling>   :state   :quit\n")
    print(f"It doesn't know you yet. Say something.   [weight x{weight}]\n")

    while True:
        try:
            text = input("you > ").strip()
        except (EOFError, KeyboardInterrupt):
            break
        if not text:
            continue
        if text in (":quit", ":q", "quit", "exit"):
            break

        if text.startswith(":why"):
            parts = text.split()
            if len(parts) < 2:
                print("   usage: :why krodha\n"); continue
            rasa = parts[1]
            entries = history.get(rasa, [])
            if not entries:
                print(f"   {rasa} has never moved.\n"); continue
            print(f"\n   every turn that moved {rasa}:")
            total = 0.0
            for t, ev, cue, delta in entries:
                total += delta
                print(f"     turn {t:<3} {ev:<20} (matched \"{cue}\")  {delta:+.4f}")
            print(f"     {'':<28}{'sum':>16}  {total:+.4f}")
            print(f"     {'':<28}{'current value':>16}  {them.state.rasa[rasa]:+.4f}")
            print("     (they differ by whatever decay removed between turns)\n")
            continue

        if text == ":state":
            print()
            for k, v in sorted(them.state.rasa.items(), key=lambda kv: -kv[1]):
                if v > 0.005:
                    print(f"   {k:<12} {bar(v)} {v:.3f}")
            print()
            continue

        event, strength, cue = classify(text)
        if not event:
            print("   READ    nothing it recognises — the classifier is 40 lines, "
                  "read it and add a word\n")
            continue

        turn += 1
        before = dict(them.state.rasa)
        for _ in range(weight):
            them.experience(event, strength, source=you)
            world.step()
        after = them.state.rasa

        moved = {k: after[k] - before[k] for k in after
                 if abs(after[k] - before[k]) >= 0.001}
        for k, d in moved.items():
            history.setdefault(k, []).append((turn, event, cue, d))

        print(f"\n   READ    \"{cue}\" -> {event} at strength {strength:.2f}"
              f"   (x{weight} moments)")
        print(f"   MOVED   ", end="")
        if moved:
            top = sorted(moved.items(), key=lambda kv: -abs(kv[1]))[:5]
            print("   ".join(f"{k} {d:+.3f}" for k, d in top))
        else:
            print("nothing above 0.001 — one event is small; the engine "
                  "accumulates")
        print(f"   STATE   ", end="")
        strongest = sorted(after.items(), key=lambda kv: -kv[1])[:3]
        print("   ".join(f"{k} {v:.2f}" for k, v in strongest))

        f = them.behavior_toward(you)
        v = them.voice_toward(you)
        print(f"   TOWARD  wants to {f.action}, tone {f.tone}, "
              f"warmth {f.warmth:+.2f}   (voice: {v.tone}/{v.color})")
        print(f"           ask why:  :why {max(moved, key=lambda k: abs(moved[k]))}\n"
              if moved else "")

    print("\nEverything you just saw was arithmetic. Run :why on any feeling and it")
    print("sums back to the turns that produced it — no weights, nothing estimated.")


if __name__ == "__main__":
    main()
