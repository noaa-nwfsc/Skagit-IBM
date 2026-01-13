//
// Created by Troy Frever on 5/19/25.
//

#include <cmath> // keep for Linux
#include <vector>
#include "fish_movement.h"
#include "model.h"
#include "hydro.h"
#include "map.h"


void FishMovement::addCurrentLocation(std::vector<std::tuple<MapNode *, float, float> > &neighbors, MapNode *point,
                                      float spentCost, float stayCost,
                                      float currentLocationFitness) const {
    neighbors.emplace_back(point, spentCost + stayCost, currentLocationFitness);
}

void FishMovement::addReachableNeighbors(std::vector<std::tuple<MapNode *, float, float> > &neighbors, MapNode *point,
                                         float spentCost, MapNode *map_node) const {
    auto reachableNeighbors = getReachableNeighbors(point, spentCost, map_node);
    neighbors.insert(
        neighbors.end(),
        std::make_move_iterator(reachableNeighbors.begin()),
        std::make_move_iterator(reachableNeighbors.end())
    );
}

float FishMovement::getCurrentU(const MapNode &node) const {
    return hydroModel->getCurrentU(node);
}

float FishMovement::getCurrentV(const MapNode &node) const {
    return hydroModel->getCurrentV(node);
}

/**
 * Calculate the dot product of two 2D vectors
 */
double dotProduct(double ax, double ay, double bx, double by) {
    return ax * bx + ay * by;
}

/**
 * Normalize a 2D vector (modifies the input variables)
 */
void normalizeVector(double &x, double &y) {
    double magnitude = std::sqrt(x * x + y * y);
    if (magnitude > 0) {
        x /= magnitude;
        y /= magnitude;
    }
}

/**
 * Calculate the effective swim speed of a fish moving between two nodes
 *
 * @param startNode The node where the fish starts
 * @param endNode The node where the fish is heading
 * @param stillWaterSwimSpeed The swim speed of the fish in still water (non-negative)
 * @return The effective swim speed (non-negative, returns 0 if fish cannot make progress)
 */
double FishMovement::calculateEffectiveSwimSpeed(const MapNode &startNode, const MapNode &endNode,
                                                 double stillWaterSwimSpeed) const {
    // Calculate direction vector from start to end
    double dirX = endNode.x - startNode.x;
    double dirY = endNode.y - startNode.y;

    normalizeVector(dirX, dirY);

    auto startNodeVelocity = hydroModel->getScaledFlowVelocityAt(startNode);
    auto endNodeVelocity = hydroModel->getScaledFlowVelocityAt(endNode);

    double uStart = static_cast<double>(startNodeVelocity.u);
    double uEnd = static_cast<double>(endNodeVelocity.u);
    double vStart = static_cast<double>(startNodeVelocity.v);
    double vEnd = static_cast<double>(endNodeVelocity.v);

    double avgU = (uStart + uEnd) / 2.0;
    double avgV = (vStart + vEnd) / 2.0;

    double waterVelocityInDirectionOfMovement = dotProduct(avgU, avgV, dirX, dirY);
    double effectiveSpeed = stillWaterSwimSpeed + waterVelocityInDirectionOfMovement;

    // Ensure the effective speed is non-negative
    return std::max(0.0, effectiveSpeed);
}

/**
 * Calculate the effective swim speed for a fish moving along an edge
 *
 * @param edge The edge along which the fish is moving
 * @param startNode Pointer to the node where the fish starts (must be either edge.nodeA or edge.nodeB)
 * @param stillWaterSwimSpeed The swim speed of the fish in still water (non-negative)
 * @return The effective swim speed (non-negative, returns 0 if fish cannot make progress)
 */
double FishMovement::calculateTransitSpeed(const Edge &edge, const MapNode *startNode,
                                           double stillWaterSwimSpeed) const {
    // Determine the end node based on the start node
    const MapNode *endNode = (startNode == edge.source) ? edge.target : edge.source;

    return calculateEffectiveSwimSpeed(*startNode, *endNode, stillWaterSwimSpeed);
}

bool FishMovement::canMoveInDirectionOfEndNode(float transitSpeed, float swimSpeed) const {
    return transitSpeed > 0.0f;
}

float FishMovement::getRemainingTime(float spentCost) const {
    float elapsedTime = spentCost / swimSpeed;
    return SECONDS_PER_TIMESTEP - elapsedTime;
}

float FishMovement::calculateStayCost(MapNode *point, float spentCost) const {
    float remainingTime = getRemainingTime(spentCost);
    float pointFlowSpeed = model.hydroModel.getUnsignedFlowSpeedAt(*point);
    return remainingTime > 0.0f ? remainingTime * pointFlowSpeed : 0.0f;
}

size_t FishMovement::selectNeighborIndex(const std::vector<std::tuple<MapNode *, float, float> > &neighbors) const {
    std::vector<float> weights;
    float totalFitness = 0.0f;
    for (const auto &neighbor : neighbors) {
        totalFitness += std::get<2>(neighbor);
    }
    for (const auto &neighbor : neighbors) {
        weights.emplace_back(std::get<2>(neighbor) / totalFitness);
    }
    return sample(weights.data(), neighbors.size());
}

std::vector<std::tuple<MapNode *, float, float> > FishMovement::getReachableNeighbors(
    MapNode *startPoint,
    float spentCost,
    MapNode *initialFishLocation
) const {
    std::vector<std::tuple<MapNode *, float, float> > neighbors;
    std::vector<Edge> allEdges;
    allEdges.reserve(startPoint->edgesIn.size() + startPoint->edgesOut.size());
    allEdges.insert(allEdges.end(), startPoint->edgesIn.begin(), startPoint->edgesIn.end());
    allEdges.insert(allEdges.end(), startPoint->edgesOut.begin(), startPoint->edgesOut.end());

    for (Edge &edge: allEdges) {
        MapNode *startNode = startPoint;
        MapNode *endNode = (startNode == edge.source ? edge.target : edge.source);
        if (model.hydroModel.getDepth(*endNode) >= MOVEMENT_DEPTH_CUTOFF) {
            float transitSpeed = (float) calculateTransitSpeed(edge, startNode, swimSpeed);
            if (canMoveInDirectionOfEndNode(transitSpeed, swimSpeed)) {
                float edgeCost = (edge.length / transitSpeed) * swimSpeed;
                if (isDistributary(endNode->type) && startPoint == initialFishLocation) {
                    edgeCost = std::min(edgeCost, swimRange - spentCost);
                }
                float totalCost = spentCost + edgeCost;
                if (totalCost <= swimRange) {
                    float fitness = fitnessCalculator(model, *endNode, totalCost);
                    neighbors.emplace_back(endNode, totalCost, fitness);
                }
            }
        }
    }
    return neighbors;
}

std::pair<MapNode *, float> FishMovement::determineNextLocation(MapNode *originalLocation) const {
    MapNode *point = originalLocation;
    float accumulatedCost = 0.0f;
    std::vector<std::tuple<MapNode *, float, float> > neighbors;
    std::vector<float> weights;
    float currentLocationFitness = fitnessCalculator(model, *point, 0.0f);
    while (true) {
        neighbors.clear();
        float remainingTime = getRemainingTime(accumulatedCost);

        if (remainingTime > 0.0f) {
            float stayCost = calculateStayCost(point, accumulatedCost);
            addCurrentLocation(neighbors, point, accumulatedCost, stayCost, currentLocationFitness);
            addReachableNeighbors(neighbors, point, accumulatedCost, originalLocation);
        }
        if (!neighbors.empty()) {
            size_t idx = selectNeighborIndex(neighbors);
            MapNode *lastPoint = point;
            point = std::get<0>(neighbors[idx]);
            accumulatedCost = std::get<1>(neighbors[idx]);
            currentLocationFitness = std::get<2>(neighbors[idx]);

            if (point == lastPoint) {
                break;
            }
        } else {
            break;
        }
    }
    return {point, accumulatedCost};
}
