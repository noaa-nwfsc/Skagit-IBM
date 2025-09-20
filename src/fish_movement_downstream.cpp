//
// Created by Troy Frever on 9/18/25.
//

#include "fish_movement_downstream.h"

FishMovementDownstream::FishMovementDownstream(Model &model, float swimSpeed, float swimRange) : FishMovement(
    model, swimSpeed, swimRange, [](Model &, MapNode &, float) -> float { return 1.0f; }) {
    fixedFitness = 1.0f;
}

bool FishMovementDownstream::canMoveInDirectionOfEndNode(float transitSpeed, float swimSpeed) const {
    return transitSpeed >= swimSpeed;
}

void FishMovementDownstream::addCurrentLocation(std::vector<std::tuple<MapNode *, float, float> > &neighbors,
                                                MapNode *point, float accumulated_cost, float stay_cost,
                                                float current_location_fitness) {
    FishMovement::addCurrentLocation(neighbors, point, accumulated_cost, stay_cost, fixedFitness);
}
