"""Tests for the Python binding.

These check the properties the library claims: determinism, that history changes how an
agent receives an identical event, that emergent states cannot be injected, and that
state survives a save/load round trip.

Run: pytest tests/ -v
"""
from __future__ import annotations

import pytest

import samskriti as sk


def test_version():
    assert sk.version() == "0.3.0"


def test_determinism_same_seed_same_hash():
    """Same seed, same calls, identical world state."""
    def run() -> int:
        with sk.World(seed=1234) as w:
            a, b = w.spawn(), w.spawn()
            a.move_to(0, 0, 0)
            b.move_to(1, 0, 0)
            for _ in range(5):
                a.experience("helped", 0.6, source=b)
                b.experience("attacked", 0.4, source=a)
                w.step()
            return w.hash

    assert run() == run()


def test_different_seed_diverges():
    def run(seed: int) -> int:
        with sk.World(seed=seed) as w:
            a = w.spawn()
            a.experience("discovery", 0.9)
            w.step(times=20)
            return w.hash

    assert run(1) != run(2)


def test_history_changes_response_to_identical_event():
    """The central claim: the same event lands differently on differently-formed agents."""
    with sk.World(seed=7) as w:
        befriended, betrayed, other = w.spawn(), w.spawn(), w.spawn()
        for agent in (befriended, betrayed, other):
            agent.move_to(0, 0, 0)

        for _ in range(10):
            befriended.experience("helped", 0.7, source=other)
            betrayed.experience("attacked", 0.7, source=other)
            w.step()
        w.step(times=10)

        warm = befriended.behavior_toward(other)
        cold = betrayed.behavior_toward(other)

        assert warm.warmth > cold.warmth, (
            f"history did not differentiate: {warm} vs {cold}")
        assert befriended.state.dominant != betrayed.state.dominant


def test_emergent_states_cannot_be_injected():
    """vishada/titiksha/vairagya form from sustained combinations — they are not events."""
    with sk.World(seed=3) as w:
        a = w.spawn()
        for name in sk.EMERGENT_RASAS:
            with pytest.raises(ValueError):
                a.experience(name, 0.9)


def test_unknown_event_rejected():
    with sk.World(seed=3) as w:
        a = w.spawn()
        with pytest.raises(ValueError, match="unknown event kind"):
            a.experience("vibed", 0.5)


def test_intensity_validated():
    with pytest.raises(ValueError):
        sk.Experience("attacked", intensity=1.5)


def test_state_shape():
    with sk.World(seed=11) as w:
        a = w.spawn()
        a.experience("received_gift", 0.6)
        w.step(times=3)
        s = a.state

        assert len(s.rasa) == 23
        assert set(s.guna) == {"sattva", "rajas", "tamas"}
        assert len(s.vasana) == 8
        assert s.dominant in sk.RASA_NAMES
        assert 0.0 <= s.prana <= 1.0
        assert s.age == 3
        # english view maps every rasa
        assert len(s.english) == 23


def test_save_load_round_trip(tmp_path):
    path = tmp_path / "world.bin"
    with sk.World(seed=99) as w:
        a, b = w.spawn(), w.spawn()
        a.move_to(0, 0, 0)
        b.move_to(1, 0, 0)
        for _ in range(8):
            a.experience("celebrated_with", 0.7, source=b)
            w.step()
        before = a.state
        w.save(str(path))

    with sk.World(seed=99) as w2:
        w2.load(str(path))
        after = sk.Agent(w2, 0).state

    assert after.rasa == before.rasa
    assert after.age == before.age
    assert after.samskara_count == before.samskara_count


def test_bond_forms_between_nearby_agents():
    """Bonds accumulate from proximity and shared experience.

    `strength` decays when interaction stops, so `peak` is what records that a bond
    existed at all.
    """
    with sk.World(seed=5) as w:
        a, b = w.spawn(), w.spawn()
        a.move_to(0, 0, 0)
        b.move_to(1, 0, 0)
        for _ in range(15):
            a.experience("celebrated_with", 0.6, source=b)
            w.step()
        assert a.bond_with(b).peak > 0.0


def test_samskaras_form_on_tick_not_on_inject():
    """Experiences are absorbed when time advances, not at injection."""
    with sk.World(seed=1) as w:
        a = w.spawn()
        a.experience("attacked", 0.8)
        assert a.state.samskara_count == 0
        w.step()
        assert a.state.samskara_count == 1


def test_agent_accepts_agent_or_int_as_source():
    with sk.World(seed=2) as w:
        a, b = w.spawn(), w.spawn()
        a.experience("helped", 0.5, source=b)      # Agent
        w.step()
        a.experience("helped", 0.5, source=b.id)   # int
        w.step()
        assert a.state.samskara_count >= 1


def test_load_restores_agents():
    """world.load() used to leave world.agents empty — the C++ side restored the souls
    but nothing rebuilt the Python list, so a loaded world looked like an empty one and
    callers silently started over with a fresh agent."""
    import os, tempfile
    import samskriti as s

    path = os.path.join(tempfile.mkdtemp(), "t.world")
    w = s.World(seed=7)
    a, b = w.spawn(), w.spawn()
    for _ in range(12):
        a.experience("helped", 0.7, source=b)
        w.step()
    before = (a.state.age, a.state.dominant, a.state.samskara_count,
              round(a.state.rasa["shringara"], 6))
    w.save(path)
    w.close()

    w2 = s.World(seed=7)
    w2.load(path)
    assert len(w2.agents) == 2, f"expected 2 agents after load, got {len(w2.agents)}"
    r = w2.agents[0].state
    assert (r.age, r.dominant, r.samskara_count, round(r.rasa["shringara"], 6)) == before
    assert w2.agents[0].bond_with(w2.agents[1]).shared_experiences == 12
    w2.close()
