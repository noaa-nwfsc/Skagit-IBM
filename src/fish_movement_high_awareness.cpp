//
// Created by Troy Frever on 12/8/25.
//

#include "fish_movement_high_awareness.h"

std::pair<MapNode *, float> FishMovementHighAwareness::determineNextLocation(MapNode *originalLocation) const {
    MapNode *point = originalLocation;
    float accumulatedCost = 0.0f;
    std::vector<std::tuple<MapNode *, float, float> > neighbors;
    std::vector<float> weights;
    float currentLocationFitness = fitnessCalculator(model, *point, 0.0f);

    float remainingTime = SECONDS_PER_TIMESTEP;

    float pointFlowSpeed = model.hydroModel.getUnsignedFlowSpeedAt(*point);
    float stayCost = remainingTime * pointFlowSpeed;

    addCurrentLocation(neighbors, point, accumulatedCost, stayCost, currentLocationFitness);

    return {point, accumulatedCost};
}
