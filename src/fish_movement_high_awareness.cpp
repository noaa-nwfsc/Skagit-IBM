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

    // 1. Setup containers
    // Priority queue stores {cost, node}, ordered by min cost
    DijkstraMinQueue dijkstraMinQueue{MinPriorityTupleComparator()};

    // Map to track lowest cost to reach a node to avoid cycles/suboptimal paths
    std::map<MapNode *, float> minCosts;

    // Store all valid reachable nodes to sample from later
    //struct Destination { MapNode* node; float cost; float fitness; };
    std::vector<std::tuple<MapNode *, float, float> > candidates;

    // 2. Initialize with start node
    minCosts[startPoint] = 0.0f;
    dijkstraMinQueue.emplace(0.0f, startPoint); // maybe start with all the neighbors instead?

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

        // todo GROT: remove duplication with FishMovement::getReachableNeighbors()
        std::vector<Edge> allEdges;
        allEdges.reserve(node->edgesIn.size() + node->edgesOut.size());
        allEdges.insert(allEdges.end(), node->edgesIn.begin(), node->edgesIn.end());
        allEdges.insert(allEdges.end(), node->edgesOut.begin(), node->edgesOut.end());

        // Explore neighbors (combining edgesIn and edgesOut)
        for (Edge &edge: allEdges) {
            MapNode *nextDest = (edge.source == node) ? edge.target : edge.source;

            // todo grot: change FishMovement::getReachableNeighbors() to match this idiom
            if (model.hydroModel.getDepth(*nextDest) < MOVEMENT_DEPTH_CUTOFF) continue;

            // todo grot: remove duplication with FishMovement::getReachableNeighbors()
            // maybe rename this method and call parent getReachableNeighbors here, then
            // process the result into the Dijkstra queue
            float transitSpeed = calculateTransitSpeed(edge, node, swimSpeed);
            if (canMoveInDirectionOfEndNode(transitSpeed, swimSpeed)) {
                float edgeCost = (edge.length / transitSpeed) * swimSpeed;
                if (isDistributary(nextDest->type) && node == startPoint) {
                    edgeCost = std::min(edgeCost, swimRange - currentCost);
                }
                float totalCost = currentCost + edgeCost;
                if (totalCost <= swimRange) {
                    const auto nextCostIter = minCosts.find(nextDest);
                    const bool isNewNode = (nextCostIter == minCosts.end());
                    const bool isCheaperPath = (!isNewNode && totalCost < nextCostIter->second);

                    if (isNewNode || isCheaperPath) {
                        minCosts[nextDest] = totalCost;
                        dijkstraMinQueue.emplace(totalCost, nextDest);
                    }
                }
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

std::pair<MapNode *, float> FishMovementHighAwareness::determineNextLocation(MapNode *originalLocation) const {
    float startingCost = 0.0f;
    std::vector<std::tuple<MapNode *, float, float> > neighbors;
    std::vector<float> weights;
    float currentLocationFitness = fitnessCalculator(model, *originalLocation, startingCost);

    float remainingTime = SECONDS_PER_TIMESTEP;
    float pointFlowSpeed = model.hydroModel.getUnsignedFlowSpeedAt(*originalLocation);
    float stayCost = remainingTime * pointFlowSpeed;
    // TODO: GROT: remove duplication above this line

    addCurrentLocation(neighbors, originalLocation, startingCost, stayCost, currentLocationFitness);
    addReachableNeighbors(neighbors, originalLocation, startingCost, nullptr);

    MapNode *point = originalLocation;
    float cost = stayCost;
    // todo grot: remove duplication with parent class sampling
    if (!neighbors.empty()) {
        weights.clear();
        float totalFitness = 0.0f;
        for (size_t i = 0; i < neighbors.size(); ++i) {
            totalFitness += std::get<2>(neighbors[i]);
        }
        for (size_t i = 0; i < neighbors.size(); ++i) {
            weights.emplace_back(std::get<2>(neighbors[i]) / totalFitness);
        }
        size_t idx = sample(weights.data(), neighbors.size());
        point = std::get<0>(neighbors[idx]);
        cost = std::get<1>(neighbors[idx]);
    }
    return {point, cost};
}
