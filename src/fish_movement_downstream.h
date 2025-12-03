//
// Created by Troy Frever on 9/18/25.
//

#ifndef FISHMOVEMENTDOWNSTREAM_H
#define FISHMOVEMENTDOWNSTREAM_H


#include "fish_movement.h"

class FishMovementDownstream : public FishMovement {
public:
    explicit FishMovementDownstream(Model &model, float swimSpeed, float swimRange);

    bool canMoveInDirectionOfEndNode(float transitSpeed, float swimSpeed) const override;
    void addCurrentLocation(std::vector<std::tuple<MapNode *, float, float> > &neighbors, MapNode * point, float spentCost, float stay_cost, float current_location_fitness) override;

private:
    float fixedFitness = 1.0f;
};


#endif // FISHMOVEMENTDOWNSTREAM_H
