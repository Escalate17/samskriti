"""ctypes bindings to the C engine.

The Python package does not reimplement the engine — it calls the same compiled C++ the
paper used, so the full dynamics (18 rasa couplings, compound emergence, per-rasa decay,
constitutional drift) are present here exactly as they are in the native library.

Internal module. Use `samskriti.Engine`.
"""
from __future__ import annotations

import ctypes
import glob
import os
import sys

from .constants import RASA_COUNT, VASANA_COUNT


# ── struct layouts — must mirror cpp/include/samskriti.h exactly ────────────
class SoulConfig(ctypes.Structure):
    _fields_ = [
        ("position", ctypes.c_float * 3),
        ("initial_rasa", ctypes.c_float * RASA_COUNT),
        ("initial_gunas", ctypes.c_float * 3),
        ("parent_a_id", ctypes.c_int),
        ("parent_b_id", ctypes.c_int),
        ("generation", ctypes.c_int),
        ("karma_rina", ctypes.c_float),
    ]


class SoulState(ctypes.Structure):
    _fields_ = [
        ("id", ctypes.c_int),
        ("is_alive", ctypes.c_int),
        ("rasa", ctypes.c_float * RASA_COUNT),
        ("gunas", ctypes.c_float * 3),
        ("dominant_rasa", ctypes.c_int),
        ("identity_coherence", ctypes.c_float),
        ("vasanas", ctypes.c_float * VASANA_COUNT),
        ("buddhi_action", ctypes.c_int),
        ("buddhi_target_id", ctypes.c_int),
        ("prana", ctypes.c_float),
        ("health", ctypes.c_float),
        ("age", ctypes.c_int),
        ("generation", ctypes.c_int),
        ("karma_rina", ctypes.c_float),
        ("samskara_count", ctypes.c_int),
        ("disposition_toward_source", ctypes.c_float),
    ]


class BondInfo(ctypes.Structure):
    _fields_ = [
        ("soul_a", ctypes.c_int),
        ("soul_b", ctypes.c_int),
        ("bond_strength", ctypes.c_float),
        ("trust", ctypes.c_float),
        ("familiarity", ctypes.c_float),
        ("emotional_resonance", ctypes.c_float),
        ("crystallized_floor", ctypes.c_float),
        ("peak_bond", ctypes.c_float),
        ("interaction_count", ctypes.c_int),
    ]


class BehaviorHint(ctypes.Structure):
    _fields_ = [
        ("action", ctypes.c_int),
        ("intensity", ctypes.c_float),
        ("tone", ctypes.c_int),
    ]


class DialogueModifiers(ctypes.Structure):
    _fields_ = [
        ("tone", ctypes.c_int),
        ("formality", ctypes.c_int),
        ("emotional_color", ctypes.c_int),
        ("tag0", ctypes.c_char * 32), ("tag0_weight", ctypes.c_float),
        ("tag1", ctypes.c_char * 32), ("tag1_weight", ctypes.c_float),
        ("tag2", ctypes.c_char * 32), ("tag2_weight", ctypes.c_float),
    ]


# ── locate and load the compiled library ────────────────────────────────────
def _find_library() -> str:
    here = os.path.dirname(os.path.abspath(__file__))
    if sys.platform == "darwin":
        patterns = ["*.dylib", "*.so"]
    elif sys.platform == "win32":
        patterns = ["*.dll", "*.pyd"]
    else:
        patterns = ["*.so"]
    for pattern in patterns:
        for candidate in sorted(glob.glob(os.path.join(here, pattern))):
            if "samskriti" in os.path.basename(candidate).lower():
                return candidate
    env = os.environ.get("SAMSKRITI_LIBRARY")
    if env and os.path.exists(env):
        return env
    raise ImportError(
        "Could not find the compiled Samskriti library.\n"
        f"Looked in: {here}\n\n"
        "Most likely you are importing from a source checkout, which shadows the\n"
        "installed package. Either run from another directory, or build in place:\n"
        "    pip install -e .\n\n"
        "Alternatively build the C++ yourself and point at the result:\n"
        "    cd cpp && cmake -B build && cmake --build build\n"
        "    export SAMSKRITI_LIBRARY=$PWD/build/libsamskriti.dylib   # .so / .dll"
    )


_lib = ctypes.CDLL(_find_library())

# ── signatures ──────────────────────────────────────────────────────────────
_lib.samskriti_version.restype = ctypes.c_char_p
_lib.samskriti_init.argtypes = [ctypes.c_uint64]
_lib.samskriti_shutdown.argtypes = []
_lib.samskriti_default_config.restype = SoulConfig
_lib.create_soul.argtypes = [SoulConfig]
_lib.create_soul.restype = ctypes.c_int
_lib.destroy_soul.argtypes = [ctypes.c_int]
_lib.get_soul_count.restype = ctypes.c_int
_lib.set_soul_position.argtypes = [ctypes.c_int, ctypes.c_float, ctypes.c_float, ctypes.c_float]
_lib.inject_event.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_float, ctypes.c_int]
_lib.tick.argtypes = [ctypes.c_float]
_lib.get_state.argtypes = [ctypes.c_int]
_lib.get_state.restype = SoulState
_lib.get_bond.argtypes = [ctypes.c_int, ctypes.c_int]
_lib.get_bond.restype = BondInfo
_lib.get_world_hash.restype = ctypes.c_uint64
_lib.save_world.argtypes = [ctypes.c_char_p]
_lib.load_world.argtypes = [ctypes.c_char_p]
_lib.get_behavior_hint.argtypes = [ctypes.c_int, ctypes.c_int]
_lib.get_behavior_hint.restype = BehaviorHint
_lib.get_dialogue_modifiers.argtypes = [ctypes.c_int, ctypes.c_int]
_lib.get_dialogue_modifiers.restype = DialogueModifiers

lib = _lib
