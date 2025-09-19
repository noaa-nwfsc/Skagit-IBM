//
// Created by Troy Frever on 9/18/25.
//

#ifndef FISHMOVEMENTDOWNSTREAM_H
#define FISHMOVEMENTDOWNSTREAM_H


#include "fish_movement.h"

class FishMovementDownstream : public FishMovement {
public:
    explicit FishMovementDownstream(Model &model, float swimSpeed, float swimRange)
        : FishMovement(model, swimSpeed, swimRange, [](Model &, MapNode &, float) -> float { return 1.0f; }) {}

    bool canMoveInDirectionOfEndNode(float transitSpeed, float swimSpeed) const override;
};


#endif // FISHMOVEMENTDOWNSTREAM_H
