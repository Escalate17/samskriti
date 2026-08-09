# Samskriti — emotional state engine

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Tests](https://img.shields.io/badge/tests-32%2F32-brightgreen.svg)](tests/test_samskriti.cpp)
[![Paper](https://img.shields.io/badge/paper-10.5281%2Fzenodo.20531430-b31b1b.svg)](https://doi.org/10.5281/zenodo.20531430)

The reference implementation for **"Character Without Retraining: Emergent Civilizational
Psychology from Continuous Emotional State Dynamics in Non-LLM Multi-Agent Simulation"**
([paper](https://doi.org/10.5281/zenodo.20531430)).

A C++17 library that gives agents persistent emotional and relational state. No neural
network, no training, no external dependencies. Character emerges from how experience
accumulates in state over time.

---

## The idea

Most artificial characters are configured: you write a personality, and the character is
that description until you rewrite it. This engine takes the opposite approach — character
is *computed*, as the residue of what an agent has lived through.

The state model is drawn from classical Indian frameworks for emotional and behavioral
dynamics, mapped onto computable quantities.

### The four concepts, for readers new to them

These terms come from Indian philosophical traditions that model inner life differently from
the way most Western computing does. The relevant difference: they treat the mind not as a
single thing that *has* states, but as a **layered system in which states arise, interact,
fade, and — if repeated — harden into disposition**. That is already a description of a
dynamical system, which is why it ports to code unusually cleanly.

**rasa** — *the felt quality of a state.* From the Natyashastra, a treatise on dramaturgy
(roughly 200 BCE–200 CE) which catalogued the affective states a performance evokes. Rasa is
not quite "emotion" in the English sense; it's closer to a distinct flavour of feeling, each
with its own character and its own natural duration. Here: a **23-dimensional state vector**,
each dimension decaying at its own rate — peace (`shanta`) fades slowly, wonder (`adbhuta`)
fades fast.

**guna** — *constitutional mode.* From Samkhya philosophy: three qualities said to compose
all phenomena in varying proportion. **Sattva** (clarity, balance), **rajas** (activity,
drive), **tamas** (inertia, density). These are not good/bad — they're modes, and every agent
is a mixture. Here the guna vector acts as a **sensitivity multiplier**: it determines *how
hard a given event lands*. A sattva-dominant agent and a tamas-dominant agent receiving the
identical event end up in different states.

**samskara** — *an impression left by experience.* Literally a groove or imprint. The idea is
that experience doesn't just pass through; it deposits something that shapes what comes next.
Here: a **stored trace** carrying valence, intensity, context, and the age at which it formed.

**vasana** — *the tendency a samskara hardens into.* When similar impressions accumulate, they
consolidate into a latent disposition that biases future behaviour without being consciously
chosen — a habit, in the strict sense. Here: **clusters formed from repeated samskaras**, and
they are what the engine reads when deciding how an agent is inclined to act.

The pipeline runs in that order: an event produces **rasa** movement, scaled by **guna**;
the event deposits a **samskara**; repeated samskaras consolidate into **vasana**; vasana
shapes how the next event is received.

This is not decorative naming. Samskara and vasana are the actual data structures, the guna
vector is a live term in the state-update equation, and the rasa decay table is taken from the
Natyashastra taxonomy rather than invented.

**The central mechanism** is that event magnitude is derived from state rather than looked
up in a table:

```
delta = direction × intensity × governing_guna × (current_rasa + 0.1)
        × guna_scale × relationship_amplifier × aversion_modifier
```

A sattva-dominant agent amplifies sattva-governed rasas more than a tamas-dominant one. An
agent already carrying fear amplifies incoming fear more than a calm one. An agent bonded to
the source of an event feels it harder in both directions. Identical input, different
outcome, determined entirely by accumulated history.

## What the paper reports

Three findings, all emergent from the dynamics rather than specified:

- **Bond strengths spanning multiple orders of magnitude** — relationships differentiate on
  their own; nothing assigns them tiers.
- **A trimodal distribution of inherited behavioral traits** — descendants cluster into three
  temperamental groups without any rule producing three of anything.
- **Population-level psychological shifts following significant loss** — grief propagates
  through the bond graph and moves the constitution of agents who were not directly involved.

![architecture](figures/fig1_architecture.png)

Figures from the paper are in [`figures/`](figures/).

## Build and run

No dependencies beyond a C++17 compiler.

```bash
# tests (32 assertions across emotional dynamics, bonds, determinism, dialogue)
clang++ -std=c++17 -O2 -Iinclude tests/test_samskriti.cpp src/*.cpp -o samskriti_tests
./samskriti_tests

# minimal example: 10 souls, one attack event, watch state diverge
clang++ -std=c++17 -O2 -Iinclude examples/minimal.cpp src/*.cpp -o minimal
./minimal
```

Or with CMake:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/samskriti_tests
```

## API

```c
samskriti_init(seed);                       // deterministic given a seed
int id = create_soul(samskriti_default_config());
inject_event(id, "attacked", 0.7f, source_id);
tick(dt);                                   // advance all souls
SoulState s = get_state(id);                // 23 rasas, 3 gunas, samskaras, bonds
BondInfo b = get_bond(a, b);
uint64_t h = get_world_hash();              // determinism check
```

`get_behavior_hint()` and `get_dialogue_modifiers()` expose the state as an action
suggestion and as tone/formality/emotional-color, which is how a renderer or a language
model consumes it without the state living inside the model.

## Where this applies

The engine is domain-agnostic: it holds relational and emotional state for an agent and
exposes it as structured output. Anywhere a character needs to *stay itself* across a long
interaction, and to be changed by what happens in it, is a candidate.

**Game and virtual characters.** The state per (character, player) pair is fixed-size and
independent of how long they've interacted — an NPC that remembers a betrayal from forty
hours ago costs the same as one you just met. That property matters at multiplayer scale,
where storing full interaction history per pair does not hold up.

**Multi-agent and social simulation.** The paper's own domain. Bonds, grief propagation, and
trait inheritance across generations emerge from the dynamics; nothing scripts them. Useful
for modelling population-level psychological effects rather than individual behaviour.

**Conversational agents.** State computed outside the language model, with the model
generating *from* the state rather than inferring it from a transcript. Demonstrated in a live
system ([meet.samskriti.app](https://meet.samskriti.app)) via a reduced Python port.

**Social and assistive robotics.** A robot in a long-term domestic or care setting has the
same problem: it should be shaped by months of interaction with a specific person, on hardware
that can't hold that history in context. The state here is small, serialisable, and
inspectable. *Untested in this domain — listed as a fit for the mechanism, not a demonstrated
result.*

**Interactive narrative.** Characters whose disposition toward the reader is computed from
what actually happened, rather than branched from flags.

A caveat worth stating plainly: these are **application domains, not validated markets**. The
engine has been demonstrated in simulation (this repo, and the paper) and in one live
conversational product. Everything else above is a reasoned fit, not evidence of adoption.

## Determinism

Given a fixed seed the engine is bit-reproducible; `get_world_hash()` returns a checksum of
world state and the test suite asserts against a known value. This matters for the thesis:
the character trajectory is a computed, replayable function of its experience history, not a
sample from a distribution.

## Open questions

**Which dynamical features are load-bearing?** A reduced Python port of this engine runs the
live conversational system linked above. It carries the state-derived sensitivity formula and
the full samskara→vasana pipeline, but omits four things present here: inter-rasa coupling
(18 relationships — wrath suppresses peace, courage counters fear), compound-emotion emergence
(despair forming out of sustained grief and delusion), per-rasa decay rates, and
constitutional guna drift over a lifetime.

That makes this a natural ablation. Two working systems, shared lineage, one richer than the
other — and an open question about how much of the richness character persistence actually
requires. Not all of it transfers cleanly: guna drift in the simulation keys off world state
(isolation, proximity to death) that has no analogue in a two-party conversation, so porting
it means redefining what constitutional pressure means for a relationship rather than copying
a function.

## What this repo is and isn't

- **Figures are published artifacts.** The run data behind them isn't included, so `figures/`
  documents results rather than regenerating them end-to-end.
- **This is the engine, not the harness.** The Godot environment, world rendering, and
  chronicle generation used to produce the paper's runs live separately.
- **Several constants are hand-tuned** and marked as such in the source. The rasa decay rates
  follow the Natyashastra taxonomy; the coupling coefficients and thresholds were set by
  observed behaviour, not derived.

## Citation

```bibtex
@misc{patel2026samskriti,
  title  = {Character Without Retraining: Emergent Civilizational Psychology from
            Continuous Emotional State Dynamics in Non-LLM Multi-Agent Simulation},
  author = {Patel, Tarang},
  year   = {2026},
  doi    = {10.5281/zenodo.20531430},
  note   = {Independent research}
}
```

## License

MIT — see [LICENSE](LICENSE). The paper is CC-BY-4.0.
