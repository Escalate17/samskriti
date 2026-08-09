"""Public data types.

Experience is what you put in; State and Bond are what you read out. Everything is a
plain dataclass — no engine internals leak through this boundary.
"""
from __future__ import annotations

from dataclasses import dataclass, field

from .constants import RASA_ENGLISH


@dataclass
class Experience:
    """One thing that happened to a character.

    You decide what counts as an experience and how to classify it. The engine decides
    what it does to the character — that part is not yours to specify, which is the
    whole point: the same Experience lands differently on differently-formed agents.

    Args:
        kind: what happened, as one of the engine's known event types. See
            `samskriti.EVENT_TYPES` for the list (e.g. "attacked", "helped",
            "received_gift", "celebrated_with", "witnessed_death").
        intensity: 0.0 (trivial) to 1.0 (profound). How much of it there was.
        source: id of the agent who caused it, or None if nobody did. Supplying this
            is what lets bonds and directed feeling form — an event with a source
            lands harder on an agent already bonded to that source.
    """
    kind: str
    intensity: float = 0.5
    source: int | None = None

    def __post_init__(self) -> None:
        if not 0.0 <= self.intensity <= 1.0:
            raise ValueError(f"intensity must be in [0,1], got {self.intensity}")


@dataclass
class State:
    """A character's complete state at one moment.

    `rasa` and `guna` are dicts keyed by Sanskrit name; `english` gives the same
    emotional readout keyed by plain-English handles if you prefer.
    """
    id: int
    alive: bool
    rasa: dict[str, float]
    guna: dict[str, float]
    dominant: str
    coherence: float          # [0..1] stability of emotional identity
    vasana: dict[str, float]  # behavioural tendencies [-1..1]
    inclination: str          # current behavioural pull
    inclination_target: int | None
    prana: float              # [0..1] vital energy
    health: float
    age: int                  # ticks lived
    generation: int
    karma_rina: float         # [-1..1] debtor/creditor polarity
    samskara_count: int       # impressions accumulated (capped at 40)
    disposition_to_source: float  # [-1..1] feeling toward the last event's source

    @property
    def english(self) -> dict[str, float]:
        """The emotional state keyed by English words instead of Sanskrit."""
        return {RASA_ENGLISH.get(k, k): v for k, v in self.rasa.items()}

    @property
    def strongest(self) -> list[tuple[str, float]]:
        """Rasas above zero, strongest first."""
        return sorted(((k, v) for k, v in self.rasa.items() if v > 0.001),
                      key=lambda kv: -kv[1])

    def __repr__(self) -> str:
        top = ", ".join(f"{k}={v:.2f}" for k, v in self.strongest[:3]) or "flat"
        return (f"State(id={self.id}, dominant={self.dominant!r}, {top}, "
                f"age={self.age}, samskaras={self.samskara_count})")


@dataclass
class Bond:
    """The relationship between two agents, as the engine has formed it.

    `strength` decays without sustained proximity and interaction, so a bond that is
    not maintained fades. `peak` records the highest it ever reached, and `crystallized`
    is a permanent floor it can no longer fall below — earned only by a bond held above
    the crystallisation threshold long enough. A crystallised bond is the engine's way
    of saying two agents were genuinely close, whatever has happened since.
    """
    strength: float
    trust: float
    familiarity: float
    resonance: float          # similarity of current emotional states
    peak: float               # highest strength ever reached
    crystallized: float       # permanent floor; 0.0 = never crystallised
    shared_experiences: int

    def __repr__(self) -> str:
        extra = f", crystallized={self.crystallized:.2f}" if self.crystallized else ""
        return (f"Bond(strength={self.strength:.3f}, peak={self.peak:.3f}, "
                f"trust={self.trust:+.2f}, shared={self.shared_experiences}{extra})")


@dataclass
class Behavior:
    """What the agent is inclined to do toward a specific other agent."""
    action: str
    intensity: float
    tone: str
    warmth: float


@dataclass
class Voice:
    """State rendered for a language model: tone, formality, colour, and tags.

    This is the handoff point. The engine computes who the character currently is;
    you decide how to phrase that into a prompt. Nothing here writes English prose
    for you — that stays your layer.
    """
    tone: str
    formality: str
    color: str
    tags: list[str] = field(default_factory=list)
