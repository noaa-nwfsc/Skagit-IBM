//
// Created by Troy Frever on 9/22/25.
//

#include "fish_movement_factory.h"
#include "fish_movement.h"
#include "fish_movement_downstream.h"
#include "fish_movement_high_awareness.h"
#include "model.h"
#include <stdexcept>


std::unique_ptr<FishMovement> FishMovementFactory::createFishMovement(
    Model &model,
    float swimSpeed,
    float swimRange,
    const std::function<float(Model &, MapNode &, float)> &fitnessCalculator,
    const ModelConfigMap &config
) {
    std::string awareness = config.getString(ModelParamKey::AgentAwareness);

    if (awareness == "low") {
        return std::make_unique<FishMovementDownstream>(model, swimSpeed, swimRange);
    }

    if (awareness == "medium") {
        return std::make_unique<FishMovement>(model, swimSpeed, swimRange, fitnessCalculator);
    }

    if (awareness == "high") {
        return std::make_unique<FishMovementHighAwareness>(model, swimSpeed, swimRange, fitnessCalculator);
    }

    throw std::runtime_error("Unknown AgentAwareness value: " + awareness);
}
