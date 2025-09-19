#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <memory>

#include "test_utilities.h"
#include "fish_movement.h"
#include "fish_movement_downstream.h"


TEST_CASE("FishMovement::calculateFishMovement tests", "[fish_movement]") {
    auto hydroModel = std::make_unique<MockHydroModel>();
    Model testModel(hydroModel.get());
    FishMovement fishMover(testModel);

    SECTION("Fish moving in still water") {
        auto nodeA = createMapNode(0.0, 0.0);
        auto nodeB = createMapNode(3.0, 4.0);;  // Creates a 3-4-5 triangle
        Edge edge(nodeA.get(), nodeB.get(), 0.0);
        
        hydroModel->uValue = 0.0f;
        hydroModel->vValue = 0.0f;
        
        double stillWaterSpeed = 1.0;
        double result = fishMover.calculateTransitSpeed(edge, nodeA.get(), stillWaterSpeed);

        REQUIRE(result == Catch::Approx(1.0));
    }
    SECTION("Fish moving with favorable current") {
        auto nodeA = createMapNode(0.0, 0.0);
        auto nodeB = createMapNode(1.0, 0.0); // Moving eastward
        Edge edge(nodeA.get(), nodeB.get(), 0.0);

        hydroModel->uValue = 0.5f; // Current flowing east
        hydroModel->vValue = 0.0f;

        double stillWaterSpeed = 1.0;
        double normalChannelResult = fishMover.calculateTransitSpeed(edge, nodeA.get(), stillWaterSpeed);

        REQUIRE(normalChannelResult == Catch::Approx(1.5)); // Speed should be increased by current

        SECTION("Same movement in blind channel has reduced current effect") {
            // Create a blind channel mock that inherits the existing hydroModel's values
            class BlindChannelMockHydroModel : public MockHydroModel {
            public:
                explicit BlindChannelMockHydroModel(const MockHydroModel* baseModel) {
                    uValue = baseModel->uValue;
                    vValue = baseModel->vValue;
                }

                float scaledFlowSpeed(float speed, const MapNode&) override {
                    return speed * 0.5f; // 50% reduction in blind channels
                }

                FlowVelocity getScaledFlowVelocityAt(const MapNode &node) override {
                    return {getCurrentU(node) * 0.5f, getCurrentV(node) * 0.5f};
                }
            };

            auto blindHydroModel = std::make_unique<BlindChannelMockHydroModel>(hydroModel.get());
            Model testModelBlind(blindHydroModel.get());

            FishMovement blindChannelFishMover(testModelBlind);

            double blindChannelResult = blindChannelFishMover.calculateTransitSpeed(edge, nodeA.get(), stillWaterSpeed);

            // With 0.5 current scaled down by 50%, the effective speed should be:
            // stillWaterSpeed(1.0) + scaledCurrent(0.25) = 1.25
            REQUIRE(blindChannelResult == Catch::Approx(1.25));
            REQUIRE(blindChannelResult < normalChannelResult);
        }

    }

    SECTION("Fish moving against strong current") {
        auto nodeA = createMapNode(0.0, 0.0);
        auto nodeB = createMapNode(1.0, 0.0); // Moving eastward
        Edge edge(nodeA.get(), nodeB.get(), 0.0);

        hydroModel->uValue = -2.0f; // Strong current flowing west
        hydroModel->vValue = 0.0f;

        double stillWaterSpeed = 1.0;
        double result = fishMover.calculateTransitSpeed(edge, nodeA.get(), stillWaterSpeed);

        REQUIRE(result == Catch::Approx(0.0)); // Fish cannot make progress
    }

    SECTION("Fish moving perpendicular to current") {
        auto nodeA = createMapNode(0.0, 0.0);
        auto nodeB = createMapNode(0.0, 1.0); // Moving northward
        Edge edge(nodeA.get(), nodeB.get(), 0.0);

        hydroModel->uValue = 1.0f; // Current flowing east
        hydroModel->vValue = 0.0f;

        double stillWaterSpeed = 1.0;
        double result = fishMover.calculateTransitSpeed(edge, nodeA.get(), stillWaterSpeed);

        REQUIRE(result == Catch::Approx(1.0)); // Perpendicular current doesn't affect speed
    }

    SECTION("Fish moving in diagonal current") {
        auto nodeA = createMapNode(0.0, 0.0);
        auto nodeB = createMapNode(1.0, 1.0); // Moving northeast
        Edge edge(nodeA.get(), nodeB.get(), 0.0);

        hydroModel->uValue = 0.5f; // Current flowing northeast
        hydroModel->vValue = 0.5f;

        double stillWaterSpeed = 1.0;
        double result = fishMover.calculateTransitSpeed(edge, nodeA.get(), stillWaterSpeed);

        REQUIRE(result > 1.0); // Speed should be increased by favorable current
    }

    SECTION("Fish movement with zero swim speed") {
        auto nodeA = createMapNode(0.0, 0.0);
        auto nodeB = createMapNode(1.0, 0.0);
        Edge edge(nodeA.get(), nodeB.get(), 0.0);

        hydroModel->uValue = 1.0f;
        hydroModel->vValue = 0.0f;

        double stillWaterSpeed = 0.0;
        double result = fishMover.calculateTransitSpeed(edge, nodeA.get(), stillWaterSpeed);

        REQUIRE(result == Catch::Approx(1.0)); // Movement only due to current
    }

    SECTION("Movement calculation is symmetric") {
        auto nodeA = createMapNode(0.0, 0.0);
        auto nodeB = createMapNode(1.0, 1.0);
        Edge edge(nodeA.get(), nodeB.get(), 0.0);

        hydroModel->uValue = 0.5f;
        hydroModel->vValue = 0.5f;

        double stillWaterSpeed = 1.0;
        double forwardResult = fishMover.calculateTransitSpeed(edge, nodeA.get(), stillWaterSpeed);
        double reverseResult = fishMover.calculateTransitSpeed(edge, nodeB.get(), stillWaterSpeed);

        double flowGoingForward = forwardResult - stillWaterSpeed;
        double flowInReverse = stillWaterSpeed - reverseResult;
        REQUIRE(flowGoingForward == Catch::Approx(flowInReverse).margin(0.0001));
    }
}

TEST_CASE("FishMovementDownstream movement restriction tests", "[fish_movement_downstream]") {
    auto hydroModel = std::make_unique<MockHydroModel>();
    Model testModel(hydroModel.get());
    FishMovementDownstream downstreamMover(testModel);

    SECTION("Fish can move with favorable current (downstream)") {
        auto nodeA = createMapNode(0.0, 0.0);
        auto nodeB = createMapNode(1.0, 0.0); // Moving eastward
        Edge edge(nodeA.get(), nodeB.get(), 0.0);

        hydroModel->uValue = 0.5f; // Current flowing east (same direction as movement)
        hydroModel->vValue = 0.0f;

        float stillWaterSpeed = 1.0f;
        float transitSpeed = (float)downstreamMover.calculateTransitSpeed(edge, nodeA.get(), stillWaterSpeed);

        // Transit speed should be 1.5 (1.0 swim + 0.5 current)
        REQUIRE(transitSpeed == Catch::Approx(1.5f));

        // Fish should be allowed to move since transit speed (1.5) >= swim speed (1.0)
        REQUIRE(downstreamMover.canMoveInDirectionOfEndNode(transitSpeed, stillWaterSpeed) == true);
    }

    SECTION("Fish cannot move against current (upstream)") {
        auto nodeA = createMapNode(0.0, 0.0);
        auto nodeB = createMapNode(1.0, 0.0); // Moving eastward
        Edge edge(nodeA.get(), nodeB.get(), 0.0);

        hydroModel->uValue = -0.5f; // Current flowing west (opposite to movement)
        hydroModel->vValue = 0.0f;

        float stillWaterSpeed = 1.0f;
        float transitSpeed = (float)downstreamMover.calculateTransitSpeed(edge, nodeA.get(), stillWaterSpeed);

        // Transit speed should be 0.5 (1.0 swim - 0.5 opposing current)
        REQUIRE(transitSpeed == Catch::Approx(0.5f));

        // Fish should NOT be allowed to move since transit speed (0.5) < swim speed (1.0)
        REQUIRE(downstreamMover.canMoveInDirectionOfEndNode(transitSpeed, stillWaterSpeed) == false);
    }

    SECTION("Fish can move in still water (no current assistance)") {
        auto nodeA = createMapNode(0.0, 0.0);
        auto nodeB = createMapNode(1.0, 0.0);
        Edge edge(nodeA.get(), nodeB.get(), 0.0);

        hydroModel->uValue = 0.0f; // No current
        hydroModel->vValue = 0.0f;

        float stillWaterSpeed = 1.0f;
        float transitSpeed = (float)downstreamMover.calculateTransitSpeed(edge, nodeA.get(), stillWaterSpeed);

        // Transit speed should equal swim speed (1.0)
        REQUIRE(transitSpeed == Catch::Approx(1.0f));

        // Fish should be allowed to move since transit speed (1.0) >= swim speed (1.0)
        REQUIRE(downstreamMover.canMoveInDirectionOfEndNode(transitSpeed, stillWaterSpeed) == true);
    }
}

TEST_CASE("FishMovementDownstream vs FishMovement behavior comparison", "[fish_movement_downstream]") {
    auto hydroModel = std::make_unique<MockHydroModel>();
    Model testModel(hydroModel.get());
    FishMovement normalMover(testModel);
    FishMovementDownstream downstreamMover(testModel);

    SECTION("Both movement types behave the same with favorable current") {
        auto nodeA = createMapNode(0.0, 0.0);
        auto nodeB = createMapNode(1.0, 0.0);
        Edge edge(nodeA.get(), nodeB.get(), 0.0);

        hydroModel->uValue = 1.0f; // Strong favorable current
        hydroModel->vValue = 0.0f;

        float stillWaterSpeed = 1.0f;
        float transitSpeed = (float)normalMover.calculateTransitSpeed(edge, nodeA.get(), stillWaterSpeed);

        // Both should allow movement with strong favorable current
        REQUIRE(normalMover.canMoveInDirectionOfEndNode(transitSpeed, stillWaterSpeed) == true);
        REQUIRE(downstreamMover.canMoveInDirectionOfEndNode(transitSpeed, stillWaterSpeed) == true);
    }

    SECTION("Different behavior against weak opposing current") {
        auto nodeA = createMapNode(0.0, 0.0);
        auto nodeB = createMapNode(1.0, 0.0);
        Edge edge(nodeA.get(), nodeB.get(), 0.0);

        hydroModel->uValue = -0.5f; // Weak opposing current
        hydroModel->vValue = 0.0f;

        float stillWaterSpeed = 1.0f;
        float transitSpeed = (float)normalMover.calculateTransitSpeed(edge, nodeA.get(), stillWaterSpeed);

        // Transit speed should be 0.5 (can still make progress)
        REQUIRE(transitSpeed == Catch::Approx(0.5f));

        // Normal movement allows any positive transit speed
        REQUIRE(normalMover.canMoveInDirectionOfEndNode(transitSpeed, stillWaterSpeed) == true);

        // Downstream-only movement requires transit speed >= swim speed
        REQUIRE(downstreamMover.canMoveInDirectionOfEndNode(transitSpeed, stillWaterSpeed) == false);
    }
}
