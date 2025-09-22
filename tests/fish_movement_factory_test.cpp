//
// Created by Troy Frever on 9/22/25.
//

#include <catch2/catch_test_macros.hpp>
#include "catch2/matchers/catch_matchers.hpp"
#include <catch2/catch_approx.hpp>
#include <memory>

#include "test_utilities.h"
#include "fish_movement_factory.h"
#include "fish_movement.h"
#include "fish_movement_downstream.h"
#include "model_config_map.h"

TEST_CASE("FishMovementFactory creates correct movement types based on AgentAwareness", "[fish_movement_factory]") {
    auto hydroModel = nullptr;
    Model testModel(hydroModel);
    auto fitnessCalculator = [](Model &, MapNode &, float) -> float { return 1.0f; };
    float swimSpeed = 1.0f;
    float swimRange = 10.0f;
    ModelConfigMap config;

    SECTION("Creates FishMovementDownstream for 'low' awareness") {
        config.set(ModelParamKey::AgentAwareness, std::string("low"));

        auto movement = FishMovementFactory::createFishMovement(
            testModel, swimSpeed, swimRange, fitnessCalculator, config
        );

        auto downstreamMovement = dynamic_cast<FishMovementDownstream*>(movement.get());
        REQUIRE(downstreamMovement != nullptr);
    }

    SECTION("Creates FishMovement for 'medium' awareness") {
        config.set(ModelParamKey::AgentAwareness, std::string("medium"));

        auto movement = FishMovementFactory::createFishMovement(
            testModel, swimSpeed, swimRange, fitnessCalculator, config
        );

        auto baseMovement = dynamic_cast<FishMovement*>(movement.get());
        auto downstreamMovement = dynamic_cast<FishMovementDownstream*>(movement.get());
        REQUIRE(baseMovement != nullptr);
        REQUIRE(downstreamMovement == nullptr);
    }

    SECTION("Throws exception for 'high' awareness (not yet supported)") {
        config.set(ModelParamKey::AgentAwareness, std::string("high"));

        REQUIRE_THROWS_WITH(
            FishMovementFactory::createFishMovement(
                testModel, swimSpeed, swimRange, fitnessCalculator, config
            ),
            "AgentAwareness 'high' is not yet supported"
        );
    }

    SECTION("Throws exception for unknown awareness value") {
        config.set(ModelParamKey::AgentAwareness, std::string("unknown"));

        REQUIRE_THROWS_WITH(
            FishMovementFactory::createFishMovement(
                testModel, swimSpeed, swimRange, fitnessCalculator, config
            ),
            "Unknown AgentAwareness value: unknown"
        );
    }
}