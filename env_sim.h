#ifndef __FISH_ENV_SIM_H
#define __FISH_ENV_SIM_H

#include <vector>
#include <unordered_map>
#include "map.h"

// "depths" and "temps" are output params, should be empty prior to calling env_sim
void env_sim(
    size_t sim_length,
    std::vector<MapNode *> &locs,
    std::vector<std::vector<float>> &depths,
    std::vector<std::vector<float>> &temps,
    float &dist_flow
);

#endif