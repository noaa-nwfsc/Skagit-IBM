#ifndef __FISH_MAP_GEN_H
#define __FISH_MAP_GEN_H

#include <vector>
#include "map.h"

void generateMap(
    std::vector<MapNode *> &dest,
    std::vector<MapNode *> &recPoints,
    int m,
    int n,
    float a,
    float p_dist,
    float p_blind
);

#endif