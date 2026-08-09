"""Samskriti — persistent emotional and relational state for AI characters.

Feed experiences in, read character state out. No LLM, no training, no network.
The same compiled C++ engine used in the paper; Python is a binding, not a reimplementation.

    import samskriti

    with samskriti.World(seed=1) as w:
        npc, player = w.spawn(), w.spawn()
        npc.experience("helped", 0.8, source=player)
        w.step(times=10)
        print(npc.behavior_toward(player))   # approach_warm

Paper: https://doi.org/10.5281/zenodo.20531430
"""
from __future__ import annotations

from .constants import (BEHAVIORS, EMERGENT_RASAS, GUNA_NAMES, RASA_ENGLISH,
                        RASA_NAMES, VASANA_NAMES)
from .engine import EVENT_TYPES, Agent, World, version
from .types import Behavior, Bond, Experience, State, Voice

__all__ = [
    "World", "Agent", "Experience", "State", "Bond", "Behavior", "Voice",
    "EVENT_TYPES", "RASA_NAMES", "RASA_ENGLISH", "EMERGENT_RASAS",
    "GUNA_NAMES", "VASANA_NAMES", "BEHAVIORS", "version",
]

__version__ = "0.3.0"
