//
// Created by Troy Frever on 8/4/25.
//

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <memory>

#include "fish_movement.h"
#include "fish_movement_downstream.h"
#include "test_utilities.h"

std::tuple<std::unique_ptr<MapNode>, std::unique_ptr<MapNode>, Edge>
createConnectedNodes(float distance, HabitatType startType = HabitatType::Distributary) {
    auto nodeA = std::make_unique<MapNode>(startType, 0.0, 0.0, 0.0);
    auto nodeB = std::make_unique<MapNode>(HabitatType::Distributary, 0.0, 0.0, 0.0);

    Edge edge(nodeA.get(), nodeB.get(), distance);
    nodeA->edgesOut.push_back(edge);
    nodeB->edgesIn.push_back(edge);

    return {std::move(nodeA), std::move(nodeB), edge};
}

TEST_CASE("getReachableNeighbors basic functionality") {
    auto hydroModel = std::make_unique<MockHydroModel>();
    Model testModel(hydroModel.get());

    // Create a fitness calculator that returns 1.0 for all cases
    auto fitnessCalculator = [](Model &, MapNode &, float) -> float { return 1.0f; };
    float swimSpeed = 1.0f;
    float swimRange = 10.0f;
    FishMovement fishMover(testModel, swimSpeed, swimRange, fitnessCalculator);

    SECTION("Empty graph (no neighbors)") {
        auto startNode = createMapNode(0.0, 0.0);
        auto result = fishMover.getReachableNeighbors(
            startNode.get(), 0.0f,
            startNode.get()
        );

        REQUIRE(result.empty());
    }

    SECTION("Single reachable neighbor") {
        auto startNode = createMapNode(0.0, 0.0);
        auto neighborNode = createMapNode(1.0, 0.0);

        connectNodes(startNode.get(), neighborNode.get(), 2.0f);

        auto result = fishMover.getReachableNeighbors(
            startNode.get(), 0.0f,
            startNode.get()
        );

        REQUIRE(result.size() == 1);

        auto [node, cost, fitness] = result[0];
        REQUIRE(node == neighborNode.get());
        REQUIRE(cost == Catch::Approx(2.0f)); // edge.length / transitSpeed * swimSpeed = 2.0/1.0*1.0
        REQUIRE(fitness == Catch::Approx(1.0f));
    }

    SECTION("Multiple reachable neighbors (some from edgesIn, some from edgesOut)") {
        auto startNode = createMapNode(0.0, 0.0);
        auto outNeighbor1 = createMapNode(1.0, 0.0);
        auto outNeighbor2 = createMapNode(2.0, 0.0);
        auto inNeighbor1 = createMapNode(-1.0, 0.0);
        auto inNeighbor2 = createMapNode(-2.0, 0.0);

        // Connect via edgesOut (startNode is source)
        connectNodes(startNode.get(), outNeighbor1.get(), 1.0f);
        connectNodes(startNode.get(), outNeighbor2.get(), 2.0f);

        // Connect via edgesIn (startNode is target)
        connectNodes(inNeighbor1.get(), startNode.get(), 1.5f);
        connectNodes(inNeighbor2.get(), startNode.get(), 2.5f);

        auto result = fishMover.getReachableNeighbors(
            startNode.get(), 0.0f,
            startNode.get()
        );

        REQUIRE(result.size() == 4);

        std::vector<MapNode *> resultNodes;
        for (const auto &[node, cost, fitness]: result) {
            resultNodes.push_back(node);
            REQUIRE(fitness == Catch::Approx(1.0f));
        }

        REQUIRE(std::find(resultNodes.begin(), resultNodes.end(), outNeighbor1.get()) != resultNodes.end());
        REQUIRE(std::find(resultNodes.begin(), resultNodes.end(), outNeighbor2.get()) != resultNodes.end());
        REQUIRE(std::find(resultNodes.begin(), resultNodes.end(), inNeighbor1.get()) != resultNodes.end());
        REQUIRE(std::find(resultNodes.begin(), resultNodes.end(), inNeighbor2.get()) != resultNodes.end());
    }

    SECTION("Multiple neighbors, some reachable and some not due to distance") {
        auto startNode = createMapNode(0.0, 0.0);
        auto closeOutNeighbor = createMapNode(1.0, 0.0, HabitatType::Nearshore);
        auto farOutNeighbor = createMapNode(2.0, 0.0, HabitatType::Nearshore);
        auto closeInNeighbor = createMapNode(-1.0, 0.0, HabitatType::Nearshore);
        auto farInNeighbor = createMapNode(-2.0, 0.0, HabitatType::Nearshore);

        // Connect neighbors with different distances
        connectNodes(startNode.get(), closeOutNeighbor.get(), 3.0f); // Cost: 3.0, reachable
        connectNodes(startNode.get(), farOutNeighbor.get(), 8.0f); // Cost: 8.0, unreachable with range 5.0
        connectNodes(closeInNeighbor.get(), startNode.get(), 2.0f); // Cost: 2.0, reachable
        connectNodes(farInNeighbor.get(), startNode.get(), 6.0f); // Cost: 6.0, unreachable with range 5.0

        const float swimRange2 = 5.0f;
        FishMovement fishMover2(testModel, swimSpeed, swimRange2, fitnessCalculator);

        auto result = fishMover2.getReachableNeighbors(
            startNode.get(), 0.0f,
            startNode.get()
        );

        REQUIRE(result.size() == 2);

        // Check that only close neighbors are present
        std::vector<MapNode *> resultNodes;
        for (const auto &[node, cost, fitness]: result) {
            resultNodes.push_back(node);
            REQUIRE(cost <= swimRange2);
        }

        REQUIRE(std::find(resultNodes.begin(), resultNodes.end(), closeOutNeighbor.get()) != resultNodes.end());
        REQUIRE(std::find(resultNodes.begin(), resultNodes.end(), closeInNeighbor.get()) != resultNodes.end());
        REQUIRE(std::find(resultNodes.begin(), resultNodes.end(), farOutNeighbor.get()) == resultNodes.end());
        REQUIRE(std::find(resultNodes.begin(), resultNodes.end(), farInNeighbor.get()) == resultNodes.end());
    }

    SECTION("All neighbors unreachable due to depth constraints") {
        auto startNode = createMapNode(0.0, 0.0);
        auto neighbor1 = createMapNode(1.0, 0.0);
        auto neighbor2 = createMapNode(-1.0, 0.0);

        connectNodes(startNode.get(), neighbor1.get(), 1.0f);
        connectNodes(neighbor2.get(), startNode.get(), 1.0f);

        // Set depth below MOVEMENT_DEPTH_CUTOFF (0.2f)
        hydroModel->depthValue = 0.1f;

        auto result = fishMover.getReachableNeighbors(
            startNode.get(), 0.0f,
            startNode.get()
        );

        REQUIRE(result.empty());
    }

    SECTION("Distributary cost calculation when startPoint == fishLocation") {
        auto startNode = createMapNode(0.0, 0.0);
        auto distributaryNeighbor = createMapNode(1.0, 0.0, HabitatType::Distributary);
        auto nearshoreNeighbor = createMapNode(-1.0, 0.0, HabitatType::Nearshore);

        connectNodes(startNode.get(), distributaryNeighbor.get(), 10.0f);
        connectNodes(startNode.get(), nearshoreNeighbor.get(), 10.0f);

        const float swimRange2 = 5.0f;
        const float currentCost = 0.0f;
        FishMovement fishMover2(testModel, swimSpeed, swimRange2, fitnessCalculator);

        auto result = fishMover2.getReachableNeighbors(
            startNode.get(), currentCost,
            startNode.get() // fishLocation == startPoint
        );

        REQUIRE(result.size() == 1);

        const auto &[node, cost, fitness] = result[0];
        REQUIRE(node->type == HabitatType::Distributary);
        REQUIRE(cost == Catch::Approx(5.0f));
    }

    SECTION("No distributary cost capping when startPoint != fishLocation") {
        auto startNode = createMapNode(0.0, 0.0);
        auto fishLocationNode = createMapNode(5.0, 5.0); // Different from startNode
        auto distributaryNeighbor = createMapNode(1.0, 0.0, HabitatType::Distributary);

        connectNodes(startNode.get(), distributaryNeighbor.get(), 10.0f);

        const float swimRange2 = 6.0f;
        const float currentCost = 0.0f;
        FishMovement fishMover2(testModel, swimSpeed, swimRange2, fitnessCalculator);

        auto result = fishMover2.getReachableNeighbors(
            startNode.get(), currentCost,
            fishLocationNode.get() // fishLocation != startPoint
        );

        // Normally a Distributary destination would have cost capped and become reachable, but only if the startNode==current location
        REQUIRE(result.empty());
    }

    SECTION("Cost capping behavior: min(edgeCost, swimRange - currentCost)") {
        auto startNode = createMapNode(0.0, 0.0);
        auto distributaryNeighbor = createMapNode(1.0, 0.0, HabitatType::Distributary);

        connectNodes(startNode.get(), distributaryNeighbor.get(), 8.0f);

        const float swimRange2 = 10.0f;
        const float currentCost = 3.0f; // swimRange - currentCost = 7.0
        FishMovement fishMover2(testModel, swimSpeed, swimRange2, fitnessCalculator);

        auto result = fishMover2.getReachableNeighbors(
            startNode.get(), currentCost,
            startNode.get() // fishLocation == startPoint
        );

        REQUIRE(result.size() == 1);

        auto [node, cost, fitness] = result[0];
        REQUIRE(node == distributaryNeighbor.get());
        // edgeCost is 8.0 + currentCost (meaning incoming current total) of 3.0 for a new total of 11
        // that is too high, so it gets lowered to the swim range
        REQUIRE(cost == Catch::Approx(10.0f));

        SECTION("Edge cost lower than remaining range") {
            const float currentCost2 = 1.0f; // swimRange - currentCost = 9.0

            auto result2 = fishMover2.getReachableNeighbors(
                startNode.get(), currentCost2,
                startNode.get()
            );

            REQUIRE(result2.size() == 1);

            auto [node2, cost2, fitness2] = result2[0];
            // Cost should be min(8.0, 9.0) + currentCost = 8.0 + 1.0 = 9.0
            REQUIRE(cost2 == Catch::Approx(9.0f));
        }
    }
}

TEST_CASE("FishMovementDownstream pathfinding restrictions", "[fish_movement_downstream][pathfinding]") {
    auto hydroModel = std::make_unique<MockHydroModel>();
    Model testModel(hydroModel.get());
    float swimSpeed = 1.0f;
    float swimRange = 10.0f;
    FishMovementDownstream downstreamMover(testModel, swimSpeed, swimRange);

    SECTION("Downstream movement allows only favorable current neighbors") {
        auto startNode = createMapNode(0.0, 0.0);
        auto westNeighbor = createMapNode(-1.0, 0.0); // West of start
        auto eastNeighbor = createMapNode(1.0, 0.0); // East of start
        auto northNeighbor = createMapNode(0.0, 1.0); // North of start

        // Connect neighbors - note the connection directions matter for flow calculation
        connectNodes(startNode.get(), westNeighbor.get(), 2.0f); // start -> west (going west)
        connectNodes(startNode.get(), eastNeighbor.get(), 2.0f); // start -> east (going east)
        connectNodes(startNode.get(), northNeighbor.get(), 2.0f); // start -> north (going north)

        // Set current flowing westward (negative U)
        hydroModel->uValue = -0.8f; // Strong westward current
        hydroModel->vValue = 0.0f;

        auto result = downstreamMover.getReachableNeighbors(
            startNode.get(), 0.0f,
            startNode.get()
        );

        // Check expected transit speeds:
        // startNode -> westNeighbor: moving west with -0.8 westward current = 1.8 transit speed (ALLOWED)
        // startNode -> eastNeighbor: moving east against -0.8 westward current = 0.2 transit speed (NOT allowed)
        // startNode -> northNeighbor: moving north with 0.0 effective current = 1.0 transit speed (ALLOWED)

        REQUIRE(result.size() == 2); // West and North neighbors should be reachable

        std::vector<MapNode *> resultNodes;
        for (const auto &[node, cost, fitness]: result) {
            resultNodes.push_back(node);
        }

        REQUIRE(std::find(resultNodes.begin(), resultNodes.end(), westNeighbor.get()) != resultNodes.end());
        REQUIRE(std::find(resultNodes.begin(), resultNodes.end(), northNeighbor.get()) != resultNodes.end());
        REQUIRE(std::find(resultNodes.begin(), resultNodes.end(), eastNeighbor.get()) == resultNodes.end());
    }

    SECTION("Comparison with normal FishMovement pathfinding behavior") {
        auto startNode = createMapNode(0.0, 0.0);
        auto westNeighbor = createMapNode(-1.0, 0.0);
        auto eastNeighbor = createMapNode(1.0, 0.0);

        connectNodes(startNode.get(), westNeighbor.get(), 3.0f);
        connectNodes(startNode.get(), eastNeighbor.get(), 3.0f);

        // Set weak westward current
        hydroModel->uValue = -0.2f; // Weak westward current
        hydroModel->vValue = 0.0f;

        auto fitnessCalc = [](Model &, MapNode &, float) -> float { return 1.0f; };

        // Test normal movement
        FishMovement normalMover(testModel, swimSpeed, swimRange, fitnessCalc);
        auto normalResult = normalMover.getReachableNeighbors(
            startNode.get(), 0.0f,
            startNode.get()
        );

        // Test downstream-only movement
        auto downstreamResult = downstreamMover.getReachableNeighbors(
            startNode.get(), 0.0f,
            startNode.get()
        );

        // Expected transit speeds:
        // West: 1.0 + 0.2 = 1.2 (assisted by current)
        // East: 1.0 - 0.2 = 0.8 (fighting against current)

        // Normal movement should allow both neighbors (both transit speeds > 0)
        REQUIRE(normalResult.size() == 2);

        // Downstream movement should only allow the westward neighbor
        // West: 1.2 >= 1.0 (allowed), East: 0.8 < 1.0 (not allowed)
        REQUIRE(downstreamResult.size() == 1);

        auto [downstreamNode, cost, fitness] = downstreamResult[0];
        REQUIRE(downstreamNode == westNeighbor.get());
    }
}
