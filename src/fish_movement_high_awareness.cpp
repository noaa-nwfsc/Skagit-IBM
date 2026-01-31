//
// Created by Troy Frever on 12/8/25.
//

#include "fish_movement_high_awareness.h"
#include <queue>
#include <map>

class MinPriorityTupleComparator {
public:
    bool operator()(const std::tuple<float, MapNode *> &lhs,
                    const std::tuple<float, MapNode *> &rhs) const {
        return std::get<0>(lhs) > std::get<0>(rhs);
    }
};

using DijkstraMinQueue = std::priority_queue<
    std::tuple<float, MapNode *>,
    std::vector<std::tuple<float, MapNode *> >,
    MinPriorityTupleComparator
>;

std::vector<std::tuple<MapNode *, float, float> > FishMovementHighAwareness::getReachableNeighbors(MapNode *startPoint,
    float spentCost, [[maybe_unused]] MapNode *initialFishLocation) const {
    // Dijkstra walk to find all nodes within swim range for this timestep, regardless of how many hops away.
    // include shortest distance (cost) for each

    DijkstraMinQueue dijkstraMinQueue{MinPriorityTupleComparator()};
    dijkstraMinQueue.emplace(0.0f, startPoint);

    std::map<MapNode *, float> minCosts;
    minCosts[startPoint] = 0.0f;

    //struct Destination { MapNode* node; float cost; float fitness; };
    std::vector<std::tuple<MapNode *, float, float> > candidates;

    // 3. Dijkstra Walk
    while (!dijkstraMinQueue.empty()) {
        auto [currentCost, node] = dijkstraMinQueue.top();
        dijkstraMinQueue.pop();

        // If we found a cheaper way to node already, skip
        auto it = minCosts.find(node);
        if (it != minCosts.end() && currentCost > it->second) continue;

        if (node != startPoint) {
            candidates.emplace_back(node, currentCost, 0.0);
        }

        auto directNeighbors = FishMovement::getReachableNeighbors(node, currentCost, startPoint);
        for (const auto &neighborTuple: directNeighbors) {
            MapNode *nextDest = std::get<0>(neighborTuple);
            float totalCost = std::get<1>(neighborTuple);

            const auto nextCostFinder = minCosts.find(nextDest);
            const bool isNewNode = (nextCostFinder == minCosts.end());
            const bool isCheaperPath = (!isNewNode && totalCost < nextCostFinder->second);

            if (isNewNode || isCheaperPath) {
                minCosts[nextDest] = totalCost;
                dijkstraMinQueue.emplace(totalCost, nextDest);
            }
        }
    }

    for (auto &candidate: candidates) {
        MapNode *node = std::get<0>(candidate);
        float candCost = std::get<1>(candidate);

        float fitness = fitnessCalculator(model, *node, candCost);
        std::get<2>(candidate) = fitness;
    }

    return candidates;
}

std::pair<MapNode *, float> FishMovementHighAwareness::determineNextLocation(MapNode *originalLocation) {
    float startingCost = 0.0f;
    std::vector<std::tuple<MapNode *, float, float> > neighbors;
    std::vector<float> weights;
    float currentLocationFitness = fitnessCalculator(model, *originalLocation, startingCost);
    float stayCost = calculateStayCost(originalLocation, startingCost);

    addCurrentLocation(neighbors, originalLocation, startingCost, stayCost, currentLocationFitness);
    addReachableNeighbors(neighbors, originalLocation, startingCost, nullptr);

    MapNode *point = originalLocation;
    float cost = stayCost;

    if (!neighbors.empty()) {
        size_t idx = selectNeighborIndex(neighbors);
        point = std::get<0>(neighbors[idx]);
        cost = std::get<1>(neighbors[idx]);
    }
    return {point, cost};
}
