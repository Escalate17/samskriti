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
dynamics, mapped onto computable quantities:

| Concept | In this engine |
|---|---|
| **rasa** | 23-dimensional emotional state vector, each dimension with its own decay rate |
| **guna** | three-way constitutional balance (sattva/rajas/tamas) that acts as a *sensitivity multiplier* — the same event lands differently on differently-constituted agents |
| **samskara** | an impression left by an experience |
| **vasana** | the tendency that hardens out of repeated samskaras — a habit, in the literal sense |

This is not decorative naming. Samskara and vasana are the actual data structures, and the
guna vector is a live term in the state-update equation.

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

## Determinism

Given a fixed seed the engine is bit-reproducible; `get_world_hash()` returns a checksum of
world state and the test suite asserts against a known value. This matters for the thesis:
the character trajectory is a computed, replayable function of its experience history, not a
sample from a distribution.

## Scope, honestly

- **This is the research engine.** A reduced Python port drives a live conversational system
  ([meet.samskriti.app](https://meet.samskriti.app)). The port carries the core
  state-derived sensitivity formula and the samskara→vasana pipeline, but omits several
  dynamical features present here — inter-rasa coupling, compound-emotion emergence,
  per-rasa decay rates, and constitutional (guna) drift. Which of those are load-bearing for
  character persistence is an open question and an active line of work.
- **Figures are paper artifacts.** The run data behind them is not in this repository, so
  `figures/` should be read as published results rather than as something the repo
  regenerates end-to-end.
- **The simulation harness is not included here.** This repo is the engine — the Godot
  environment, world rendering, and chronicle generation used to produce the paper's runs
  live separately.
- Several constants are tuned by hand and documented as such in the source. They are
  defensible but not derived.

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
