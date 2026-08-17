"""The Python API.

A `World` holds agents and steps time. An `Agent` is a handle to one character in it.

The engine is a process-wide singleton in C, so `World` is a context manager that owns
that lifetime. Create one, make agents, feed experiences, read state.
"""
from __future__ import annotations

import ctypes
from typing import Iterator

from . import _native
from ._native import lib
from .constants import (BEHAVIORS, BUDDHI_ACTIONS, GUNA_NAMES, RASA_NAMES,
                        VASANA_NAMES)
from .types import Behavior, Bond, Experience, State, Voice

#: Event kinds the engine understands. Anything else raises.
EVENT_TYPES: frozenset[str] = frozenset({
    "attacked", "helped", "betrayed", "abandoned", "gift", "received_gift",
    "reunion", "discovery", "witnessed_death", "celebrated_with",
    "saw_threat", "entered_sacred_space",
})

_TONES = ("hostile", "cold", "neutral", "warm", "familiar")
_FORMALITY = ("distant", "formal", "casual", "intimate")
_COLORS = ("fearful", "grieving", "joyful", "angry", "peaceful", "protective", "wary")


def version() -> str:
    """Version of the underlying C engine."""
    return lib.samskriti_version().decode()


class Agent:
    """A handle to one character inside a `World`."""

    __slots__ = ("_id", "_world")

    def __init__(self, world: "World", soul_id: int) -> None:
        self._id = soul_id
        self._world = world

    @property
    def id(self) -> int:
        return self._id

    def experience(self, event: Experience | str, intensity: float = 0.5,
                   source: "Agent | int | None" = None) -> None:
        """Apply one experience to this agent.

        Accepts either an `Experience`, or the same fields inline::

            npc.experience("attacked", 0.8, source=player)
            npc.experience(Experience("helped", 0.6, source=player.id))
        """
        if isinstance(event, str):
            event = Experience(event, intensity, _as_id(source))
        if event.kind not in EVENT_TYPES:
            raise ValueError(
                f"unknown event kind {event.kind!r}. Known kinds: "
                f"{', '.join(sorted(EVENT_TYPES))}")
        src = _as_id(source) if source is not None else event.source
        lib.inject_event(self._id, event.kind.encode(), ctypes.c_float(event.intensity),
                         -1 if src is None else int(src))

    @property
    def state(self) -> State:
        """Snapshot of everything the engine currently holds about this agent."""
        s = lib.get_state(self._id)
        return State(
            id=s.id,
            alive=bool(s.is_alive),
            rasa={name: round(s.rasa[i], 6) for i, name in enumerate(RASA_NAMES)},
            guna={name: round(s.gunas[i], 6) for i, name in enumerate(GUNA_NAMES)},
            dominant=RASA_NAMES[s.dominant_rasa] if 0 <= s.dominant_rasa < len(RASA_NAMES) else "unknown",
            coherence=s.identity_coherence,
            vasana={name: round(s.vasanas[i], 6) for i, name in enumerate(VASANA_NAMES)},
            inclination=BUDDHI_ACTIONS[s.buddhi_action] if 0 <= s.buddhi_action < len(BUDDHI_ACTIONS) else "unknown",
            inclination_target=None if s.buddhi_target_id < 0 else s.buddhi_target_id,
            prana=s.prana,
            health=s.health,
            age=s.age,
            generation=s.generation,
            karma_rina=s.karma_rina,
            samskara_count=s.samskara_count,
            disposition_to_source=s.disposition_toward_source,
        )

    def bond_with(self, other: "Agent | int") -> Bond:
        """The relationship this agent has formed with another."""
        b = lib.get_bond(self._id, _as_id(other))
        return Bond(strength=b.bond_strength, trust=b.trust,
                    familiarity=b.familiarity, resonance=b.emotional_resonance,
                    peak=b.peak_bond, crystallized=b.crystallized_floor,
                    shared_experiences=b.interaction_count)

    def behavior_toward(self, other: "Agent | int") -> Behavior:
        """What this agent is inclined to do toward another, and how warmly."""
        h = lib.get_behavior_hint(self._id, _as_id(other))
        return Behavior(
            action=BEHAVIORS[h.action] if 0 <= h.action < len(BEHAVIORS) else "unknown",
            intensity=h.intensity,
            tone=_TONES[h.tone] if 0 <= h.tone < len(_TONES) else "unknown",
            warmth=(h.tone - 2) / 2.0,
        )

    def voice_toward(self, other: "Agent | int") -> Voice:
        """State rendered for dialogue selection or an LLM prompt.

        Returns tone, formality, emotional colour, and up to three weighted tags.
        It does not write prose — that is your layer.
        """
        d = lib.get_dialogue_modifiers(self._id, _as_id(other))
        tags = []
        for raw, w in ((d.tag0, d.tag0_weight), (d.tag1, d.tag1_weight), (d.tag2, d.tag2_weight)):
            text = raw.decode(errors="ignore").strip("\x00").strip()
            if text:
                tags.append(f"{text}:{w:.2f}")
        return Voice(
            tone=_TONES[d.tone] if 0 <= d.tone < len(_TONES) else "unknown",
            formality=_FORMALITY[d.formality] if 0 <= d.formality < len(_FORMALITY) else "unknown",
            color=_COLORS[d.emotional_color] if 0 <= d.emotional_color < len(_COLORS) else "unknown",
            tags=tags,
        )

    def move_to(self, x: float, y: float, z: float = 0.0) -> None:
        """Set spatial position. Bonds form between agents that are near each other."""
        lib.set_soul_position(self._id, ctypes.c_float(x), ctypes.c_float(y), ctypes.c_float(z))

    def __repr__(self) -> str:
        s = self.state
        return f"<Agent {self._id} dominant={s.dominant} age={s.age}>"


class World:
    """Owns the engine and the agents in it.

    Deterministic given a seed: the same seed and the same sequence of calls produce
    byte-identical state, verifiable via `World.hash`.

        with samskriti.World(seed=42) as w:
            npc = w.spawn()
            npc.experience("helped", 0.8)
            w.step(1.0)
            print(npc.state.dominant)
    """

    def __init__(self, seed: int = 0) -> None:
        self._agents: list[Agent] = []
        self._open = False
        self.seed = seed
        self.open()

    def open(self) -> "World":
        if not self._open:
            lib.samskriti_init(ctypes.c_uint64(self.seed))
            self._open = True
        return self

    def close(self) -> None:
        if self._open:
            lib.samskriti_shutdown()
            self._open = False
            self._agents.clear()

    def __enter__(self) -> "World":
        return self.open()

    def __exit__(self, *exc: object) -> None:
        self.close()

    def spawn(self, *, position: tuple[float, float, float] = (0.0, 0.0, 0.0),
              rasa: dict[str, float] | None = None,
              guna: dict[str, float] | None = None,
              parents: tuple[int, int] | None = None,
              generation: int = 0) -> Agent:
        """Create an agent.

        `rasa` and `guna` seed the starting temperament; omit them for defaults.
        `parents` enables inheritance — traits and karmic polarity carry down.
        """
        cfg = lib.samskriti_default_config()
        for i, v in enumerate(position):
            cfg.position[i] = v
        if rasa:
            for name, v in rasa.items():
                if name not in RASA_NAMES:
                    raise ValueError(f"unknown rasa {name!r}")
                cfg.initial_rasa[RASA_NAMES.index(name)] = v
        if guna:
            for name, v in guna.items():
                if name not in GUNA_NAMES:
                    raise ValueError(f"unknown guna {name!r}")
                cfg.initial_gunas[GUNA_NAMES.index(name)] = v
        if parents:
            cfg.parent_a_id, cfg.parent_b_id = int(parents[0]), int(parents[1])
        cfg.generation = generation
        agent = Agent(self, lib.create_soul(cfg))
        self._agents.append(agent)
        return agent

    def step(self, dt: float = 1.0, times: int = 1) -> None:
        """Advance time. Decay, coupling, emergence, and drift all happen here."""
        for _ in range(times):
            lib.tick(ctypes.c_float(dt))

    @property
    def hash(self) -> int:
        """Checksum of world state — equal hashes mean identical worlds."""
        return int(lib.get_world_hash())

    @property
    def agents(self) -> list[Agent]:
        return list(self._agents)

    def save(self, path: str) -> None:
        lib.save_world(str(path).encode())

    def load(self, path: str) -> None:
        """Restore a saved world, including its agents.

        The C++ side restores souls on its own, but nothing was rebuilding the
        Python-side list, so `world.agents` came back empty on a loaded world and
        callers concluded the save had failed. Anything written as
        `world.load(p); agent = world.agents[0]` raised IndexError; anything written
        defensively as `agents[0] if agents else world.spawn()` silently started over
        with a brand-new agent, which is worse — the save appears to work and the
        history is quietly gone.

        Souls are addressed by dense integer ids, and a dead slot reports
        `is_alive == 0`, so scan upward until we've collected the count the engine
        reports. The bound is a safety net against a corrupt count, not an agent cap.
        """
        lib.load_world(str(path).encode())
        self._agents.clear()
        expected = int(lib.get_soul_count())
        probe, limit = 0, max(expected * 4, expected + 1024)
        while len(self._agents) < expected and probe < limit:
            if lib.get_state(probe).is_alive:
                self._agents.append(Agent(self, probe))
            probe += 1

    def __len__(self) -> int:
        return int(lib.get_soul_count())

    def __iter__(self) -> Iterator[Agent]:
        return iter(self._agents)

    def __repr__(self) -> str:
        return f"<World seed={self.seed} agents={len(self._agents)} hash={self.hash:016x}>"


def _as_id(a: "Agent | int | None") -> int | None:
    if a is None:
        return None
    return a.id if isinstance(a, Agent) else int(a)
