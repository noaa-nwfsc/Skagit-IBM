//
// Created by Troy Frever on 12/8/25.
//

#ifndef HEADLESS_GUI_FISHMOVEMENTHIGHAWARENESS_H
#define HEADLESS_GUI_FISHMOVEMENTHIGHAWARENESS_H

#include "fish_movement.h"

class FishMovementHighAwareness : public FishMovement {
public:
    explicit FishMovementHighAwareness(Model &model, float swimSpeed, float swimRange,
                          const std::function<float(Model &, MapNode &, float)> &fitnessCalculator)
    : FishMovement(model, swimSpeed, swimRange, fitnessCalculator) {}

    std::pair<MapNode *, float> determineNextLocation(MapNode *originalLocation) const override;
};


#endif //HEADLESS_GUI_FISHMOVEMENTHIGHAWARENESS_H