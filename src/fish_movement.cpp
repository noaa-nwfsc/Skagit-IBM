//
// Created by Troy Frever on 5/19/25.
//

#include <cmath> // keep for Linux
#include <vector>
#include "fish_movement.h"
#include "model.h"
#include "hydro.h"
#include "map.h"

float FishMovement::getCurrentU(const MapNode& node) const {
    return hydroModel->getCurrentU(node);
}
float FishMovement::getCurrentV(const MapNode& node) const {
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
void normalizeVector(double& x, double& y) {
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
double FishMovement::calculateEffectiveSwimSpeed(const MapNode& startNode, const MapNode& endNode, double stillWaterSwimSpeed) const {
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
double FishMovement::calculateTransitSpeed(const Edge& edge, const MapNode* startNode, double stillWaterSwimSpeed) const {
    // Determine the end node based on the start node
    const MapNode* endNode = (startNode == edge.source) ? edge.target : edge.source;

    return calculateEffectiveSwimSpeed(*startNode, *endNode, stillWaterSwimSpeed);
}

bool FishMovement::canMoveInDirectionOfEndNode(float transitSpeed, float swimSpeed) const {
    return transitSpeed > 0.0f;
}

std::vector<std::tuple<MapNode*, float, float>> FishMovement::getReachableNeighbors(
    MapNode* startPoint,
    float swimSpeed,
    float swimRange,
    float currentCost,
    MapNode* fishLocation,
    const std::function<float(Model&, MapNode&, float)>& fitnessCalculator
) const {
    std::vector<std::tuple<MapNode*, float, float>> neighbors;
    std::vector<Edge> allEdges;
    allEdges.reserve(startPoint->edgesIn.size() + startPoint->edgesOut.size());
    allEdges.insert(allEdges.end(), startPoint->edgesIn.begin(), startPoint->edgesIn.end());
    allEdges.insert(allEdges.end(), startPoint->edgesOut.begin(), startPoint->edgesOut.end());

    for (Edge &edge: allEdges) {
        MapNode* startNode = startPoint;
        MapNode* endNode = (startNode == edge.source ? edge.target : edge.source);
        if (model.hydroModel.getDepth(*endNode) >= MOVEMENT_DEPTH_CUTOFF) {
            float transitSpeed = (float) calculateTransitSpeed(edge, startNode, swimSpeed);
            if (canMoveInDirectionOfEndNode(transitSpeed, swimSpeed)) {
                float edgeCost = (edge.length / transitSpeed) * swimSpeed;
                if (isDistributary(endNode->type) && startPoint == fishLocation) {
                    edgeCost = std::min(edgeCost, swimRange - currentCost);
                }
                if (currentCost + edgeCost <= swimRange) {
                    float fitness = fitnessCalculator(model, *endNode, currentCost + edgeCost);
                    neighbors.emplace_back(endNode, currentCost + edgeCost, fitness);
                }
            }
        }
    }
    return neighbors;
}
