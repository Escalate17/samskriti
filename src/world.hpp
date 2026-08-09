#pragma once
#include <vector>
#include <unordered_map>
#include "soul.hpp"
#include "bonds.hpp"
#include "rng.hpp"

struct World {
    std::vector<Soul>    souls;
    BondMap              bonds;
    int                  next_soul_id = 0;
    int                  sim_tick     = 0;   /* total ticks elapsed */
    int                  sim_day      = 0;

    /* Lookup: soul_id → index in souls[] */
    std::unordered_map<int, int> id_to_index;

    Soul*  find_soul(int id);
    Bond*  find_bond(int a, int b);

    int    create_soul(const SoulConfig& cfg);
    void   destroy_soul(int id);
    void   set_position(int id, float x, float y, float z);
    void   inject_event(int id, const char* evt, float intensity, int src);
    void   tick(float dt);

    void   save(const char* path) const;
    void   load(const char* path);
    uint64_t hash() const;

    void   clear();
};

extern World g_world;
