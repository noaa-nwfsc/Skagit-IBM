//
// Created by Troy Frever on 5/19/25.
//

#include <cmath> // keep for Linux
#include <vector>
#include "fish_movement.h"

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
