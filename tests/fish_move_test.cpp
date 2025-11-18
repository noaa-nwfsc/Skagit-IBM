//
// Created by Troy Frever on 11/17/25.
//

#include <catch2/catch_test_macros.hpp>
#include <memory>

#include "fish.h"
#include "model.h"
#include "test_utilities.h"

class TestFish : public Fish {
public:
    using Fish::Fish;

    float getFitness(Model &, MapNode &, float) override {
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
};

TEST_CASE("Fish::move stays at current location when there are no neighbors", "[fish][move]") {
    MoveTestFixture fixture; // default Distributary node, no edges

    TestFish fish(
        /*id*/        0UL,
        /*spawnTime*/ 0L,
        /*forkLength*/50.0f,
        /*location*/  fixture.node.get()
    );

    // Non-fatal environment
    fixture.hydroModel->depthValue = 1.0f;  // positive depth → no stranding
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
        /*id*/        1UL,
        /*spawnTime*/ 0L,
        /*forkLength*/50.0f,
        /*location*/  nodeA.get()
    );

    hydroModel->depthValue = 1.0f;
    hydroModel->uValue = 0.0f;
    hydroModel->vValue = 0.0f;

    // Neighbor list is [stay at A, neighbor B, ...]; index 0 is always "stay".
    auto staySampler = [](float *weights, unsigned weightsLen) -> unsigned {
        (void)weights;
        (void)weightsLen;
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
        /*id*/        2UL,
        /*spawnTime*/ 0L,
        /*forkLength*/50.0f,
        /*location*/  nodeA.get()
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
        /*id*/        3UL,
        /*spawnTime*/ 0L,
        /*forkLength*/forkLength,
        /*location*/  nodeA.get()
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