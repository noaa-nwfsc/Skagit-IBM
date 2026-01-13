//
// Created by Troy Frever on 1/13/26.
//

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include <vector>

#include "fish_movement_high_awareness.h"
#include "test_utilities.h"

TEST_CASE("FishMovementHighAwareness::determineNextLocation integration", "[fish_movement][high_awareness]") {
    auto hydroModel = std::make_unique<MockHydroModel>();
    Model testModel(hydroModel.get());

    auto startNode = createMapNode(0.0, 0.0);
    auto endNode = createMapNode(10.0, 0.0);

    constexpr float edgeLength = 10.0f;
    connectNodes(startNode.get(), endNode.get(), edgeLength);

    // With 0 flow, TransitSpeed == SwimSpeed
    float swimSpeed = 1.0f;
    float transitSpeed = swimSpeed;
    float swimRange = 20.0f;

    constexpr float startFitness = 1.0f;
    constexpr float endFitness = 9.0f;

    auto fitnessCalc = [&](Model &, MapNode &node, float) -> float {
        if (&node == startNode.get()) return startFitness;
        if (&node == endNode.get()) return endFitness;
        return 0.0f;
    };

    FishMovementHighAwareness mover(testModel, swimSpeed, swimRange, fitnessCalc);

    // Verify weights and force selection of the neighbor (EndNode)
    auto sampler = [](float *weights, unsigned weightsLen) -> unsigned {
        CHECK(weightsLen == 2);

        float totalFitness = startFitness + endFitness;
        float expectedStartWeight = startFitness / totalFitness;
        float expectedEndWeight = endFitness / totalFitness;

        REQUIRE(weights[0] == Catch::Approx(expectedStartWeight));
        REQUIRE(weights[1] == Catch::Approx(expectedEndWeight));

        return 1; // Select index 1 (EndNode)
    };
    SampleOverrideHelper override(sampler);

    auto result = mover.determineNextLocation(startNode.get());

    REQUIRE(result.first == endNode.get());

    float expectedCost = (edgeLength / transitSpeed) * swimSpeed;
    REQUIRE(result.second == Catch::Approx(expectedCost));
}

TEST_CASE("FishMovementHighAwareness::getReachableNeighbors multi-hop reachability", "[fish_movement][high_awareness]") {
    auto hydroModel = std::make_unique<MockHydroModel>();
    Model testModel(hydroModel.get());

    // Graph: A -> B -> C (Reachable) and B -> D (Unreachable)
    auto nodeA = createMapNode(0.0, 0.0);
    auto nodeB = createMapNode(10.0, 0.0);
    auto nodeC = createMapNode(20.0, 0.0);
    auto nodeD = createMapNode(10.0, 100.0); // Far away

    // Edges
    connectNodes(nodeA.get(), nodeB.get(), 10.0f); // A -> B (Cost 10)
    connectNodes(nodeB.get(), nodeC.get(), 10.0f); // B -> C (Cost 10)
    connectNodes(nodeB.get(), nodeD.get(), 100.0f); // B -> D (Cost 100)

    float swimSpeed = 1.0f;
    float swimRange = 25.0f; // Enough for A->B (10) and A->B->C (20), but not A->B->D (110)

    auto fitnessCalc = [](Model &, MapNode &, float) -> float { return 1.0f; };
    FishMovementHighAwareness mover(testModel, swimSpeed, swimRange, fitnessCalc);

    auto results = mover.getReachableNeighbors(nodeA.get(), 0.0f, nodeA.get());

    // Extract reachable nodes
    std::vector<MapNode *> reachableNodes;
    for (const auto &tuple: results) {
        reachableNodes.push_back(std::get<0>(tuple));
    }

    // Verify Reachability
    // B (direct) and C (multi-hop) should be reachable
    REQUIRE_THAT(reachableNodes, Catch::Matchers::VectorContains(nodeB.get()));
    REQUIRE_THAT(reachableNodes, Catch::Matchers::VectorContains(nodeC.get()));

    // D should NOT be reachable (too far)
    REQUIRE_THAT(reachableNodes, !Catch::Matchers::VectorContains(nodeD.get()));

    // A (Start Node) should NOT be in the reachable list
    REQUIRE_THAT(reachableNodes, !Catch::Matchers::VectorContains(nodeA.get()));

    for (const auto &tuple: results) {
        auto node = std::get<0>(tuple);
        float cost = std::get<1>(tuple);

        if (node == nodeB.get()) {
            REQUIRE(cost == Catch::Approx(10.0f));
        }
        if (node == nodeC.get()) {
            REQUIRE(cost == Catch::Approx(20.0f));
        }
    }
}

TEST_CASE("FishMovementHighAwareness::getReachableNeighbors Dijkstra shortest path selection", "[fish_movement][high_awareness]") {
    auto hydroModel = std::make_unique<MockHydroModel>();
    Model testModel(hydroModel.get());

    auto nodeA = createMapNode(0.0, 0.0);
    auto nodeB = createMapNode(10.0, 0.0);
    auto nodeC = createMapNode(5.0, 5.0);

    // Path 1: A -> B (Direct, Expensive)
    constexpr float directDistance = 20.0f;
    connectNodes(nodeA.get(), nodeB.get(), directDistance);

    // Path 2: A -> C -> B (Indirect, Cheaper)
    constexpr float step1Distance = 5.0f;
    constexpr float step2Distance = 5.0f;
    connectNodes(nodeA.get(), nodeC.get(), step1Distance);
    connectNodes(nodeC.get(), nodeB.get(), step2Distance);

    constexpr float expectedCheaperCost = step1Distance + step2Distance;

    float swimSpeed = 1.0f;
    float swimRange = 30.0f; // Sufficient for both paths

    auto fitnessCalc = [](Model &, MapNode &, float) -> float { return 1.0f; };
    FishMovementHighAwareness mover(testModel, swimSpeed, swimRange, fitnessCalc);

    auto results = mover.getReachableNeighbors(nodeA.get(), 0.0f, nodeA.get());

    bool foundB = false;
    for (const auto &tuple: results) {
        auto node = std::get<0>(tuple);
        float cost = std::get<1>(tuple);

        if (node == nodeB.get()) {
            foundB = true;
            REQUIRE(cost == Catch::Approx(expectedCheaperCost));
        }
    }
    REQUIRE(foundB);
}

TEST_CASE("FishMovementHighAwareness constraints and logic safety net", "[fish_movement][high_awareness][constraints]") {
    auto hydroModel = std::make_unique<MockHydroModel>();
    Model testModel(hydroModel.get());

    float swimSpeed = 1.0f;
    float swimRange = 20.0f;
    auto fitnessCalc = [](Model &, MapNode &, float) -> float { return 1.0f; };
    FishMovementHighAwareness mover(testModel, swimSpeed, swimRange, fitnessCalc);

    SECTION("Path blocked by insufficient depth") {
        // Setup: A -> B -> C
        // B has insufficient depth. C is reachable via B but should be blocked.
        auto nodeA = createMapNode(0.0, 0.0);
        auto nodeB = createMapNode(10.0, 0.0);
        auto nodeC = createMapNode(20.0, 0.0);

        connectNodes(nodeA.get(), nodeB.get(), 5.0f);
        connectNodes(nodeB.get(), nodeC.get(), 5.0f);

        hydroModel->depthValue = MOVEMENT_DEPTH_CUTOFF - 0.1f;

        auto results = mover.getReachableNeighbors(nodeA.get(), 0.0f, nodeA.get());

        REQUIRE(results.empty());
    }

    SECTION("Path blocked by strong opposing current") {
        // Setup: A -> B with strong upstream current (Transit Speed <= 0)
        auto nodeA = createMapNode(0.0, 0.0);
        auto nodeB = createMapNode(10.0, 0.0); // B is East of A (+x)

        connectNodes(nodeA.get(), nodeB.get(), 10.0f);

        hydroModel->depthValue = MOVEMENT_DEPTH_CUTOFF + 1.0f;

        // To move A->B (East), we need +u. Opposing is -u.
        float largeOpposingFlow = -2.0f * swimSpeed;
        hydroModel->uValue = largeOpposingFlow;
        hydroModel->vValue = 0.0f;

        auto results = mover.getReachableNeighbors(nodeA.get(), 0.0f, nodeA.get());

        REQUIRE(results.empty());
    }
}