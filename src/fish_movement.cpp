//
// Created by Troy Frever on 5/19/25.
//

#include "fish_movement.h"

#include <vector>

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
 * Calculate the distance between two points
 */
double calculateDistance(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    return std::sqrt(dx * dx + dy * dy);
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

    // Normalize the direction vector
    normalizeVector(dirX, dirY);

    // Calculate the average water velocity along the edge
    double avgU = (getCurrentU(startNode) + getCurrentU(endNode)) / 2.0;
    double avgV = (getCurrentV(startNode) + getCurrentV(endNode)) / 2.0;

    // Calculate the component of water velocity in the direction of fish movement
    double waterVelocityComponent = dotProduct(avgU, avgV, dirX, dirY);

    // Calculate effective swim speed
    double effectiveSpeed = stillWaterSwimSpeed + waterVelocityComponent;

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

    // Use the helper function for calculation
    return calculateEffectiveSwimSpeed(*startNode, *endNode, stillWaterSwimSpeed);
}

/**
 * More accurate version that divides the edge into segments to account for
 * varying water velocity along the edge
 *
 * @param edge The edge along which the fish is moving
 * @param startNode Pointer to the node where the fish starts (must be either edge.nodeA or edge.nodeB)
 * @param stillWaterSwimSpeed The swim speed of the fish in still water (non-negative)
 * @param numSegments Number of segments to divide the edge into
 * @return The effective swim speed (non-negative, returns 0 if fish cannot make progress in any segment)
 */
double FishMovement::calculateFishMovementAdvanced(const Edge& edge, const MapNode* startNode,
                                    double stillWaterSwimSpeed, int numSegments = 10)  const {
    // Determine the end node based on the start node
    const MapNode* endNode = (startNode == edge.source) ? edge.target : edge.source;

    // Calculate direction vector and distance
    double dirX = endNode->x - startNode->x;
    double dirY = endNode->y - startNode->y;
    double totalDistance = calculateDistance(startNode->x, startNode->y, endNode->x, endNode->y);

    // Normalize the direction vector
    normalizeVector(dirX, dirY);

    double segmentLength = totalDistance / numSegments;
    double totalTime = 0.0;

    for (int i = 0; i < numSegments; ++i) {
        // Calculate interpolation factor (0 at startNode, 1 at endNode)
        double t = (i + 0.5) / numSegments;

        // Linearly interpolate water velocity at this segment
        double uStart = getCurrentU(*startNode);
        double uEnd = getCurrentU(*endNode);
        double vStart = getCurrentV(*startNode);
        double vEnd = getCurrentV(*endNode);
        double uInterp = uStart + t * (uEnd - uStart);
        double vInterp = vStart + t * (vEnd - vStart);

        // Calculate water velocity component along the path
        double waterVelComponent = dotProduct(uInterp, vInterp, dirX, dirY);

        // Calculate effective speed in this segment
        double segmentEffectiveSpeed = stillWaterSwimSpeed + waterVelComponent;

        // Check if fish can make progress in this segment
        if (segmentEffectiveSpeed <= 0) {
            return 0.0; // Fish cannot make progress against the current
        }

        // Add time to traverse this segment
        totalTime += segmentLength / segmentEffectiveSpeed;
    }

    // Calculate average effective speed
    double avgEffectiveSpeed = totalDistance / totalTime;

    return avgEffectiveSpeed;
}
