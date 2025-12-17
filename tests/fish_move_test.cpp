//
// Created by Troy Frever on 11/17/25.
//

#include <catch2/catch_test_macros.hpp>
#include <memory>

#include "fish.h"
#include "fish_movement.h"
#include "fish_movement_factory.h"
#include "model.h"
#include "test_utilities.h"
#include "catch2/catch_approx.hpp"
#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_vector.hpp"

class TestFish : public Fish {
public:
    using Fish::Fish;

    std::function<float(MapNode &)> fitnessFn;

    float getFitness(Model &, MapNode &node, float) override {
        if (fitnessFn) {
            return fitnessFn(node);
        }
        return 1.0f; // simple, deterministic fitness
    }
};

struct MoveTestFixture {
    std::unique_ptr<MockHydroModel> hydroModel;
    std::unique_ptr<Model> model;
    std::unique_ptr<MapNode> node;

    explicit MoveTestFixture(HabitatType type = HabitatType::Distributary) {
        hydroModel = std::make_unique<MockHydroModel>();
        model = std::make_unique<Model>(hydroModel.get());
        node = createMapNode(0.0f, 0.0f, type);
    }

    void setAgentAwareness(const std::string &awareness) {
        // Nasty cast to modify private/const member for testing without changing production code interface
        const ModelConfigMap &config = model->getConfigMap();
        ModelConfigMap &mutableConfig = const_cast<ModelConfigMap &>(config);
        mutableConfig.set(ModelParamKey::AgentAwareness, awareness);
    }
};

TEST_CASE("Fish::move stays at current location when there are no neighbors", "[fish][move]") {
    MoveTestFixture fixture; // default Distributary node, no edges

    TestFish fish(
        /*id*/ 0UL,
               /*spawnTime*/ 0L,
               /*forkLength*/50.0f,
               /*location*/ fixture.node.get()
    );

    // Non-fatal environment
    fixture.hydroModel->depthValue = 1.0f; // positive depth → no stranding
    fixture.hydroModel->uValue = 0.0f;
    fixture.hydroModel->vValue = 0.0f;

    bool result = fish.move(*fixture.model);

    REQUIRE(result == true);
    REQUIRE(fish.location == fixture.node.get());
}

class SampleOverrideHelper {
public:
    explicit SampleOverrideHelper(SampleFunction fn) : previous_(::sampleOverrideForTesting) {
        ::sampleOverrideForTesting = fn;
    }

    ~SampleOverrideHelper() {
        ::sampleOverrideForTesting = previous_;
    }

private:
    SampleFunction previous_;
};

// Test 1.2: Termination - staying is optimal
// Setup: Node with a neighbor, but sample() always selects the "stay" option (index 0).
// Expect: Loop exits on first iteration, fish stays at current location.
TEST_CASE("Fish::move terminates immediately when stay option is selected", "[fish][move][loop]") {
    auto hydroModel = std::make_unique<MockHydroModel>();
    Model model(hydroModel.get());

    auto nodeA = createMapNode(0.0f, 0.0f);
    auto nodeB = createMapNode(1.0f, 0.0f);
    connectNodes(nodeA.get(), nodeB.get(), 1.0f);

    TestFish fish(
        /*id*/ 1UL,
               /*spawnTime*/ 0L,
               /*forkLength*/50.0f,
               /*location*/ nodeA.get()
    );

    hydroModel->depthValue = 1.0f;
    hydroModel->uValue = 0.0f;
    hydroModel->vValue = 0.0f;

    // Neighbor list is [stay at A, neighbor B, ...]; index 0 is always "stay".
    auto staySampler = [](float *weights, unsigned weightsLen) -> unsigned {
        (void) weights;
        (void) weightsLen;
        return 0U; // Always choose "stay"
    };
    SampleOverrideHelper override(staySampler);

    bool result = fish.move(model);

    REQUIRE(result == true);
    REQUIRE(fish.location == nodeA.get());
}

// Test 1.3: Termination - selected same location after a move
// Setup: Two nodes connected both ways. First iteration moves A -> B,
// second iteration selects "stay" on B (same location), causing loop exit.
TEST_CASE("Fish::move terminates when selecting the same location in a later iteration", "[fish][move][loop]") {
    auto hydroModel = std::make_unique<MockHydroModel>();
    Model model(hydroModel.get());

    auto nodeA = createMapNode(0.0f, 0.0f);
    auto nodeB = createMapNode(1.0f, 0.0f);

    // Bi-directional connection: A <-> B
    connectNodes(nodeA.get(), nodeB.get(), 1.0f);
    connectNodes(nodeB.get(), nodeA.get(), 1.0f);

    TestFish fish(
        /*id*/ 2UL,
               /*spawnTime*/ 0L,
               /*forkLength*/50.0f,
               /*location*/ nodeA.get()
    );

    hydroModel->depthValue = 1.0f;
    hydroModel->uValue = 0.0f;
    hydroModel->vValue = 0.0f;

    // On each iteration, neighbor list ordering is:
    //   0: stay at current node
    //   1: move to the other node (from FishMovement::addReachableNeighbors)
    //
    // We want: first iteration -> index 1 (A -> B), second iteration -> index 0 (stay on B).
    static std::vector<unsigned> sequence{1U, 0U};
    static size_t seqPos = 0;

    auto sequencedSampler = [](float *weights, unsigned weightsLen) -> unsigned {
        CHECK(weightsLen == 3U);
        CHECK(seqPos < sequence.size());
        return sequence[seqPos++];
    };
    seqPos = 0;
    SampleOverrideHelper override(sequencedSampler);

    bool result = fish.move(model);

    REQUIRE(result == true);
    // After first iteration we moved to B, second iteration stayed at B and terminated
    REQUIRE(fish.location == nodeB.get());
}


// Test 1.4: Remaining time exhausted
// Setup: Edge length chosen so that moving once consumes the entire swim range.
// After the first move, remainingTime <= 0, no neighbors are added, and the loop exits.
TEST_CASE("Fish::move terminates when remaining time is exhausted", "[fish][move][loop]") {
    auto hydroModel = std::make_unique<MockHydroModel>();
    Model model(hydroModel.get());

    auto nodeA = createMapNode(0.0f, 0.0f);
    auto nodeB = createMapNode(1.0f, 0.0f);

    float forkLength = 50.0f;
    TestFish fish(
        /*id*/ 3UL,
               /*spawnTime*/ 0L,
               /*forkLength*/forkLength,
               /*location*/ nodeA.get()
    );

    // Compute swimRange exactly as Fish::move does, but locally for the test.
    float swimSpeed = swimSpeedFromForkLength(forkLength);
    float swimRange = swimSpeed * SECONDS_PER_TIMESTEP;

    // Connect A -> B with an edge whose cost will be equal to swimRange
    // when transitSpeed == swimSpeed.
    connectNodes(nodeA.get(), nodeB.get(), swimRange);

    hydroModel->depthValue = 1.0f;
    hydroModel->uValue = 0.0f;
    hydroModel->vValue = 0.0f;

    // First iteration: choose neighbor B (index 1). After this,
    // accumulatedCost ≈ swimRange, so remainingTime ≈ 0 and neighbors
    // will not be added in the second loop iteration.
    auto chooseNeighbor = [](float *weights, unsigned weightsLen) -> unsigned {
        CHECK(weightsLen == 2U);
        return 1U; // choose B (non-stay option)
    };
    SampleOverrideHelper override(chooseNeighbor);

    bool result = fish.move(model);

    REQUIRE(result == true);
    REQUIRE(fish.location == nodeB.get());
}

TEST_CASE("Fish::move selects neighbors based on normalized fitness weights", "[fish][move][selection]") {
    MoveTestFixture fixture;
    // Ensure environment is safe so fish doesn't die/strand
    fixture.hydroModel->depthValue = 1.0f;
    fixture.hydroModel->uValue = 0.0f;
    fixture.hydroModel->vValue = 0.0f;

    auto nodeA = fixture.node.get();
    auto nodeB = createMapNode(1.0f, 0.0f);
    auto nodeC = createMapNode(2.0f, 0.0f);

    // Connect neighbors. Order: A->B (index 1), A->C (index 2)
    // Index 0 is always "stay" at A
    connectNodes(nodeA, nodeB.get(), 1.0f);
    connectNodes(nodeA, nodeC.get(), 1.0f);

    TestFish fish(
        /*id*/ 100UL,
               /*spawnTime*/ 0L,
               /*forkLength*/50.0f,
               /*location*/ nodeA
    );

    // Setup: 3 neighbors with fitnesses [1.0, 2.0, 3.0]
    // Fitness mapping: Stay(A)=1.0, B=2.0, C=3.0
    fish.fitnessFn = [&](MapNode &n) {
        if (&n == nodeA) return 1.0f;
        if (&n == nodeB.get()) return 2.0f;
        if (&n == nodeC.get()) return 3.0f;
        return 0.0f;
    };

    // Shared static call counter for the non-capturing lambdas below
    static int sampleCallCount = 0;
    sampleCallCount = 0;

    SECTION("Verify weights and select index 0 (Stay)") {
        // Expect weights [1/6, 2/6, 3/6]
        auto sampler = [](float *weights, unsigned weightsLen) -> unsigned {
            sampleCallCount++;
            if (sampleCallCount == 1) {
                CHECK(weightsLen == 3);
                if (weightsLen >= 3) {
                    REQUIRE(weights[0] == Catch::Approx(1.0f / 6.0f).margin(0.0001f));
                    REQUIRE(weights[1] == Catch::Approx(2.0f / 6.0f).margin(0.0001f));
                    REQUIRE(weights[2] == Catch::Approx(3.0f / 6.0f).margin(0.0001f));
                }
                return 0; // Stay -> loop terminates
            }
            return 0;
        };
        SampleOverrideHelper override(sampler);

        bool result = fish.move(*fixture.model);
        REQUIRE(result == true);
        REQUIRE(fish.location == nodeA);
    }

    SECTION("Verify weights and select index 1 (Node B)") {
        // Move to B, then stay
        auto sampler = [](float *weights, unsigned weightsLen) -> unsigned {
            sampleCallCount++;
            if (sampleCallCount == 1) {
                CHECK(weightsLen == 3);
                if (weightsLen >= 3) {
                    REQUIRE(weights[0] == Catch::Approx(1.0f / 6.0f).margin(0.0001f));
                    REQUIRE(weights[1] == Catch::Approx(2.0f / 6.0f).margin(0.0001f));
                    REQUIRE(weights[2] == Catch::Approx(3.0f / 6.0f).margin(0.0001f));
                }
                return 1; // Move to B
            }
            return 0; // Stay at B -> loop terminates
        };
        SampleOverrideHelper override(sampler);

        bool result = fish.move(*fixture.model);
        REQUIRE(result == true);
        REQUIRE(fish.location == nodeB.get());
    }

    SECTION("Verify weights and select index 2 (Node C)") {
        // Move to C, then stay
        auto sampler = [](float *weights, unsigned weightsLen) -> unsigned {
            sampleCallCount++;
            if (sampleCallCount == 1) {
                CHECK(weightsLen == 3);
                if (weightsLen >= 3) {
                    REQUIRE(weights[0] == Catch::Approx(1.0f / 6.0f).margin(0.0001f));
                    REQUIRE(weights[1] == Catch::Approx(2.0f / 6.0f).margin(0.0001f));
                    REQUIRE(weights[2] == Catch::Approx(3.0f / 6.0f).margin(0.0001f));
                }
                return 2; // Move to C
            }
            return 0; // Stay at C -> loop terminates
        };
        SampleOverrideHelper override(sampler);

        bool result = fish.move(*fixture.model);
        REQUIRE(result == true);
        REQUIRE(fish.location == nodeC.get());
    }
}

TEST_CASE("Fish::move calculates weights correctly for equal fitness neighbors", "[fish][move][selection]") {
    MoveTestFixture fixture;
    fixture.hydroModel->depthValue = 1.0f;

    auto nodeA = fixture.node.get();
    auto nodeB = createMapNode(1.0f, 0.0f);
    auto nodeC = createMapNode(2.0f, 0.0f);

    // Connect neighbors. Order: A->B (index 1), A->C (index 2)
    connectNodes(nodeA, nodeB.get(), 1.0f);
    connectNodes(nodeA, nodeC.get(), 1.0f);

    TestFish fish(
        /*id*/ 101UL,
               /*spawnTime*/ 0L,
               /*forkLength*/50.0f,
               /*location*/ nodeA
    );

    // Setup: 3 neighbors with equal fitness (Stay=1.0, B=1.0, C=1.0)
    fish.fitnessFn = [&](MapNode &n) { return 1.0f; };

    // Expect weights [1/3, 1/3, 1/3]
    auto sampler = [](float *weights, unsigned weightsLen) -> unsigned {
        CHECK(weightsLen == 3);

        REQUIRE(weights[0] == Catch::Approx(1.0f / 3.0f).margin(0.0001f));
        REQUIRE(weights[1] == Catch::Approx(1.0f / 3.0f).margin(0.0001f));
        REQUIRE(weights[2] == Catch::Approx(1.0f / 3.0f).margin(0.0001f));
        return 0;
    };
    SampleOverrideHelper override(sampler);

    bool result = fish.move(*fixture.model);
    REQUIRE(result == true);
}

TEST_CASE("Fish::move calculates weights correctly for one dominant fitness neighbor", "[fish][move][selection]") {
    MoveTestFixture fixture;
    fixture.hydroModel->depthValue = 1.0f;

    auto nodeA = fixture.node.get();
    auto nodeB = createMapNode(1.0f, 0.0f);
    auto nodeC = createMapNode(2.0f, 0.0f);

    // Connect neighbors. Order: A->B (index 1), A->C (index 2)
    connectNodes(nodeA, nodeB.get(), 1.0f);
    connectNodes(nodeA, nodeC.get(), 1.0f);

    TestFish fish(
        /*id*/ 102UL,
               /*spawnTime*/ 0L,
               /*forkLength*/50.0f,
               /*location*/ nodeA
    );

    // Setup: Fitnesses [10.0, 0.1, 0.1]
    // Stay(A)=10.0, B=0.1, C=0.1
    // Total fitness = 10.2
    // Weight A = 10.0 / 10.2 ≈ 0.98039
    // Weight B = 0.1 / 10.2 ≈ 0.00980
    // Weight C = 0.1 / 10.2 ≈ 0.00980
    fish.fitnessFn = [&](MapNode &n) {
        if (&n == nodeA) return 10.0f;
        return 0.1f;
    };

    auto sampler = [](float *weights, unsigned weightsLen) -> unsigned {
        CHECK(weightsLen == 3);

        float totalFitness = 10.2f;
        REQUIRE(weights[0] == Catch::Approx(10.0f / totalFitness).margin(0.0001f));
        REQUIRE(weights[1] == Catch::Approx(0.1f / totalFitness).margin(0.0001f));
        REQUIRE(weights[2] == Catch::Approx(0.1f / totalFitness).margin(0.0001f));
        return 0;
    };
    SampleOverrideHelper override(sampler);

    bool result = fish.move(*fixture.model);
    REQUIRE(result == true);
}

TEST_CASE("Fish::move uses different movement strategies based on configuration", "[fish][move][strategy]") {
    MoveTestFixture fixture;
    fixture.hydroModel->depthValue = 1.0f;

    auto nodeA = fixture.node.get();
    auto nodeB = createMapNode(10.0f, 0.0f);
    auto nodeC = createMapNode(-10.0f, 0.0f);

    // Connect A -> B (Outgoing from A)
    Edge edgeOut(nodeA, nodeB.get(), 1.0f);
    nodeA->edgesOut.push_back(edgeOut);
    nodeB->edgesIn.push_back(edgeOut);

    // Connect C -> A (Incoming to A)
    Edge edgeIn(nodeC.get(), nodeA, 1.0f);
    nodeC->edgesOut.push_back(edgeIn);
    nodeA->edgesIn.push_back(edgeIn);

    // Set flow to move from Left to Right (+x)
    // This makes moving to C (left) "upstream" and moving to B (right) "downstream"
    fixture.hydroModel->uValue = 0.1f;

    TestFish fish(
        /*id*/ 200UL,
               /*spawnTime*/ 0L,
               /*forkLength*/200.0f, // choose a long enough fork length to make swim speed exceed downstream flow
               /*location*/ nodeA
    );

    static unsigned optionCount = 0;
    auto sampler = [](float *, unsigned weightsLen) -> unsigned {
        optionCount = weightsLen;
        return 0; // Stay
    };
    SampleOverrideHelper override(sampler);

    SECTION("Medium Awareness (Default) considers upstream and downstream") {
        fixture.setAgentAwareness("medium");
        optionCount = 0;

        fish.move(*fixture.model);

        // medium awareness considers incoming edges (C) as reachable:
        REQUIRE(optionCount == 3);
    }

    SECTION("Low Awareness considers only downstream") {
        fixture.setAgentAwareness("low");
        optionCount = 0;

        fish.move(*fixture.model);

        // low awareness does NOT consider incoming edge (C) as reachable:
        REQUIRE(optionCount == 2);
    }
}

TEST_CASE("FishMovement getReachableNeighbors respects agentAwareness", "[fish_movement]") {
    MoveTestFixture fixture;

    // Set flow so that A is upstream, C and D are downstream.
    // A is West of B (-x). C is East of B (+x). D is South of B (-y).
    // Flow (0.1, -0.1) goes South-East.
    fixture.hydroModel->uValue = 0.1f; // give enough flow to correctly detect direction
    fixture.hydroModel->vValue = -0.1f;

    auto nodeB = fixture.node.get();

    // Construct a simple map A->B->C that also includes B->D and D->E
    // Since nodeB is at (0,0), we adjust other nodes relative to it
    auto nodeA = createMapNode(-10.0f, 0.0f);
    auto nodeC = createMapNode(10.0f, 0.0f);
    auto nodeD = createMapNode(0.0f, -10.0f);
    auto nodeE = createMapNode(0.0f, -20.0f);

    // Connect nodes: A->B, B->C, B->D, D->E
    connectNodes(nodeA.get(), nodeB, 10.0f);
    connectNodes(nodeB, nodeC.get(), 10.0f);
    connectNodes(nodeB, nodeD.get(), 10.0f);
    connectNodes(nodeD.get(), nodeE.get(), 10.0f);

    // Create a fish at node B with sufficient swim range
    Fish fish(1, 0, 100.0f, nodeB);
    float swimSpeed = swimSpeedFromForkLength(fish.forkLength);
    float swimRange = swimSpeed * SECONDS_PER_TIMESTEP;
    auto fitnessCalc = [](Model &, MapNode &, float) { return 1.0f; };

    SECTION("agentAwareness set to medium") {
        fixture.setAgentAwareness("medium");
        auto movement = FishMovementFactory::createFishMovement(
            *fixture.model,
            swimSpeed,
            swimRange,
            fitnessCalc,
            fixture.model->getConfigMap()
        );

        auto neighbors = movement->getReachableNeighbors(nodeB, 0.0f, nodeB);

        std::vector<MapNode *> neighborNodes;
        for (const auto &n: neighbors) {
            neighborNodes.push_back(std::get<0>(n));
        }

        REQUIRE(neighborNodes.size() == 3);
        // Verify A, C, and D are present (upstream and downstream)
        REQUIRE_THAT(neighborNodes, Catch::Matchers::VectorContains(nodeA.get()));
        REQUIRE_THAT(neighborNodes, Catch::Matchers::VectorContains(nodeC.get()));
        REQUIRE_THAT(neighborNodes, Catch::Matchers::VectorContains(nodeD.get()));

        // Node E should not be in the results because it is two steps away
        REQUIRE_THAT(neighborNodes, !Catch::Matchers::VectorContains(nodeE.get()));
    }

    SECTION("agentAwareness set to low") {
        fixture.setAgentAwareness("low");
        auto movement = FishMovementFactory::createFishMovement(
            *fixture.model,
            swimSpeed,
            swimRange,
            fitnessCalc,
            fixture.model->getConfigMap()
        );

        auto neighbors = movement->getReachableNeighbors(nodeB, 0.0f, nodeB);

        std::vector<MapNode *> neighborNodes;
        for (const auto &n: neighbors) {
            neighborNodes.push_back(std::get<0>(n));
        }

        REQUIRE(neighborNodes.size() == 2);
        // Verify C and D are present (downstream)
        REQUIRE_THAT(neighborNodes, Catch::Matchers::VectorContains(nodeC.get()));
        REQUIRE_THAT(neighborNodes, Catch::Matchers::VectorContains(nodeD.get()));

        // Node A should be absent because it is upstream
        REQUIRE_THAT(neighborNodes, !Catch::Matchers::VectorContains(nodeA.get()));

        // Node E should be absent because it is two steps away
        REQUIRE_THAT(neighborNodes, !Catch::Matchers::VectorContains(nodeE.get()));
    }

    SECTION("agentAwareness set to high") {
        fixture.setAgentAwareness("high");
        auto movement = FishMovementFactory::createFishMovement(
            *fixture.model,
            swimSpeed,
            swimRange,
            fitnessCalc,
            fixture.model->getConfigMap()
        );

        auto neighbors = movement->getReachableNeighbors(nodeB, 0.0f, nodeB);

        std::vector<MapNode *> neighborNodes;
        for (const auto &n: neighbors) {
            neighborNodes.push_back(std::get<0>(n));
        }

        REQUIRE(neighborNodes.size() == 4);
        // Verify A, C, and D are present (upstream and downstream)
        REQUIRE_THAT(neighborNodes, Catch::Matchers::VectorContains(nodeA.get()));
        REQUIRE_THAT(neighborNodes, Catch::Matchers::VectorContains(nodeC.get()));
        REQUIRE_THAT(neighborNodes, Catch::Matchers::VectorContains(nodeD.get()));

        // Node E should also be in the results because "high awareness" finds it even when it is two steps away
        REQUIRE_THAT(neighborNodes, Catch::Matchers::VectorContains(nodeE.get()));
    }
}
