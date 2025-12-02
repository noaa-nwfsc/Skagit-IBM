//
// Created by Troy Frever on 9/22/25.
//

#include "fish_movement_factory.h"
#include "fish_movement.h"
#include "fish_movement_downstream.h"
#include "model.h"
#include <stdexcept>

std::unique_ptr<FishMovement> FishMovementFactory::createFishMovement(
    Model& model,
    float swimSpeed,
    float swimRange,
    const std::function<float(Model&, MapNode&, float)>& fitnessCalculator,
    const ModelConfigMap& config
) {
    std::string awareness = config.getString(ModelParamKey::AgentAwareness);

    if (awareness == "low") {
        return std::make_unique<FishMovementDownstream>(model, swimSpeed, swimRange);
    }
    else if (awareness == "medium") {
        return std::make_unique<FishMovement>(model, swimSpeed, swimRange, fitnessCalculator);
    }
    else if (awareness == "high") {
        throw std::runtime_error("AgentAwareness 'high' is not yet supported");
    }
    else {
        throw std::runtime_error("Unknown AgentAwareness value: " + awareness);
    }
}