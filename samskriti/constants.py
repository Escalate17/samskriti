"""Names for the engine's state dimensions.

The C core indexes rasas and vasanas by integer. These tables give them names on the
Python side so state reads as a dict rather than an array of 23 floats.
"""
from __future__ import annotations

# ── the 23 rasas, in C index order (see cpp/include/samskriti.h) ────────────
# Sanskrit name -> the closest English handle. Where English has no single word,
# the gloss describes the felt quality rather than forcing a translation.
RASA_NAMES: tuple[str, ...] = (
    "shanta",      # 0  peace, equanimity — the equilibrium state
    "shringara",   # 1  love, beauty, attraction
    "veera",       # 2  heroism, courage
    "karuna",      # 3  compassion, empathy
    "adbhuta",     # 4  wonder, curiosity
    "bhayanaka",   # 5  fear
    "bibhatsa",    # 6  disgust, revulsion
    "hasya",       # 7  joy, playfulness
    "raudra",      # 8  righteous fury (distinct from destructive wrath)
    "kama",        # 9  desire, craving
    "krodha",      # 10 destructive wrath
    "lobha",       # 11 greed, possessiveness
    "moha",        # 12 delusion, attachment to illusion
    "mada",        # 13 pride, arrogance
    "matsarya",    # 14 envy
    "shoka",       # 15 grief from a specific loss
    "irshya",      # 16 jealousy toward a rival bond
    "dvesha",      # 17 crystallised aversion / hatred
    "abhimana",    # 18 wounded pride
    "udvega",      # 19 chronic anxiety (sustained fear converts here)
    "vishada",     # 20 despair — emerges, cannot be injected
    "titiksha",    # 21 endurance under suffering — emerges
    "vairagya",    # 22 transcendent detachment — emerges
)

RASA_ENGLISH: dict[str, str] = {
    "shanta": "peace", "shringara": "love", "veera": "courage",
    "karuna": "compassion", "adbhuta": "wonder", "bhayanaka": "fear",
    "bibhatsa": "disgust", "hasya": "joy", "raudra": "righteous anger",
    "kama": "desire", "krodha": "wrath", "lobha": "greed",
    "moha": "delusion", "mada": "pride", "matsarya": "envy",
    "shoka": "grief", "irshya": "jealousy", "dvesha": "aversion",
    "abhimana": "wounded pride", "udvega": "anxiety", "vishada": "despair",
    "titiksha": "endurance", "vairagya": "detachment",
}

# States that can only FORM from sustained combinations — injecting them directly
# is not possible by design. See "compound emergence" in the README.
EMERGENT_RASAS: frozenset[str] = frozenset({"vishada", "titiksha", "vairagya", "udvega"})

GUNA_NAMES: tuple[str, ...] = ("sattva", "rajas", "tamas")

# ── the 8 behavioural tendencies ────────────────────────────────────────────
VASANA_NAMES: tuple[str, ...] = (
    "seek_souls", "avoid_souls", "explore", "hoard",
    "teach", "rest", "guard_territory", "follow_bonded",
)

BUDDHI_ACTIONS: tuple[str, ...] = (
    "wander", "seek_soul", "flee", "follow_bond", "rest",
    "teach", "guard_territory", "hoard", "bond_with",
)

BEHAVIORS: tuple[str, ...] = (
    "approach_warm", "approach_neutral", "retreat_wary", "retreat_fearful",
    "ignore", "aggress", "mourn", "rest",
)

RASA_COUNT = len(RASA_NAMES)
VASANA_COUNT = len(VASANA_NAMES)
