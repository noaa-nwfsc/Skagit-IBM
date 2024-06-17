#include "env_sim.h"

#include <cmath>
#include "util.h"

// "depths" and "temps" are output params, should be empty prior to calling env_sim
void env_sim(
    size_t sim_length,
    std::vector<MapNode *> &locs,
    std::vector<std::vector<float>> &depths,
    std::vector<std::vector<float>> &temps,
    float &dist_flow
) {

    dist_flow = 440.0f;
    const float dist_temp = 14.0f;
    const float temp_sd = 5.0f;

    const float dist_depth = 2.0f;
    const float depth_sd = 0.7f;

    const float max_amplitude = 2.5f/2.0f;

    // Simulate depth data

    float mean_depth = dist_depth/2.0f;
    float depth_period = M_PI;

    float temp_period = M_PI / 2.0f;
    float mean_offset = dist_temp;

    float min_dist = locs[0]->pathDist;
    float max_dist = min_dist;
    for (auto it = locs.begin(); it != locs.end(); ++it) {
        if ((*it)->pathDist < min_dist) {
            min_dist = (*it)->pathDist;
        }
        if ((*it)->pathDist > max_dist) {
            max_dist = (*it)->pathDist;
        }
    }
    
    for (MapNode *node : locs) {
        std::vector<float> nodeDepths;
        std::vector<float> nodeTemps;
        for (size_t t = 0; t < sim_length; ++t) {
            if (isDistributary(node->type)) {
                nodeDepths.push_back(dist_depth);
                nodeTemps.push_back(dist_temp);
            } else {
                float percent_influenced = 1.0f - (node->pathDist - min_dist)/(max_dist-min_dist);
                float amplitude = max_amplitude * percent_influenced;
                float tide = amplitude * sin(depth_period * ((float) t) + depth_period/2.0f) + mean_depth;
                float foo = unit_normal_rand() * depth_sd + tide;
                nodeDepths.push_back(foo);
                float temp = temp_sd * sin(temp_period * ((float) t)) + mean_offset;
                if (temp < 2.0f) {
                    temp = 2.0f;
                }
                if (temp > 39.0f) {
                    temp = 39.0f;
                }
                nodeTemps.push_back(temp);
            }
        }
        depths.push_back(nodeDepths);
        temps.push_back(nodeTemps);
    }

}