//
// Created by Troy Frever on 9/18/25.
//

#include "fish_movement_downstream.h"

bool FishMovementDownstream::canMoveInDirectionOfEndNode(float transitSpeed, float swimSpeed) const {
    return transitSpeed >= swimSpeed;
}