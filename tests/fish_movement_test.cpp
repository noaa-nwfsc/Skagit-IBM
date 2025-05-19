#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "fish_movement.h"
#include <memory>

class MockHydroModel : public HydroModel {
public:
    MockHydroModel() : HydroModel(empty_nodes_, empty_depths_, empty_temps_, 0.0) {}
    // Override the getCurrentU and getCurrentV methods
    float getCurrentU(const MapNode& node) const override { return uValue; }
    float getCurrentV(const MapNode& node) const override { return vValue; }
    
    // Values to be set in tests
    float uValue = 0.0f;
    float vValue = 0.0f;

private:
    std::vector<MapNode *> empty_nodes_;
    std::vector<std::vector<float>> empty_depths_;
    std::vector<std::vector<float>> empty_temps_;
};

auto createMapNode(float x, float y) {
    auto mapNode = std::make_unique<MapNode>(HabitatType::Distributary, 0.0, 0.0, 0.0);
    mapNode->x = x;
    mapNode->y = y;
    return mapNode;
}

TEST_CASE("FishMovement::calculateFishMovement tests", "[fish_movement]") {
    auto hydroModel = std::make_unique<MockHydroModel>();
    FishMovement fishMover(hydroModel.get());

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
        double result = fishMover.calculateTransitSpeed(edge, nodeA.get(), stillWaterSpeed);

        REQUIRE(result == Catch::Approx(1.5)); // Speed should be increased by current
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