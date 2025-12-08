//
// Created by Troy Frever on 9/18/25.
//

#include "fish_movement_downstream.h"

FishMovementDownstream::FishMovementDownstream(Model &model, float swimSpeed, float swimRange) : FishMovement(
    model, swimSpeed, swimRange, [](Model &, MapNode &, float) -> float { return 1.0f; }) {
    fixedFitness = 1.0f;
}

bool FishMovementDownstream::isTravelDirectionDownstream(float transitSpeed, float swimSpeed) const {
    return transitSpeed >= swimSpeed;
}

bool FishMovementDownstream::canMoveInDirectionOfEndNode(float transitSpeed, float swimSpeed) const {
    return isTravelDirectionDownstream(transitSpeed, swimSpeed);
}

void FishMovementDownstream::addCurrentLocation(std::vector<std::tuple<MapNode *, float, float> > &neighbors,
                                                MapNode *point, float spentCost, float stay_cost,
                                                [[maybe_unused]] float current_location_fitness) {
    FishMovement::addCurrentLocation(neighbors, point, spentCost, stay_cost, fixedFitness);
}
