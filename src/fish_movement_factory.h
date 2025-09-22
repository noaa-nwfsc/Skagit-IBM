//
// Created by Troy Frever on 9/22/25.
//

#ifndef FISH_MOVEMENT_FACTORY_H
#define FISH_MOVEMENT_FACTORY_H


#include <memory>
#include <functional>
#include "fish_movement.h"
#include "model_config_map.h"

class Model;
class MapNode;

class FishMovementFactory {
public:
    static std::unique_ptr<FishMovement> createFishMovement(
        Model& model,
        float swimSpeed,
        float swimRange,
        const std::function<float(Model&, MapNode&, float)>& fitnessCalculator,
        const ModelConfigMap& config
    );

};


#endif //FISH_MOVEMENT_FACTORY_H