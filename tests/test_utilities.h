//
// Created by Troy Frever on 8/4/25.
//

#ifndef TEST_UTILITIES_H
#define TEST_UTILITIES_H

#include <memory>
#include <functional>

#include "hydro.h"
#include "map.h"
#include "model.h"

class MockHydroModel : public HydroModel {
public:
    MockHydroModel() : HydroModel(empty_nodes_, empty_depths_, empty_temps_, 0.0) {}

    float getCurrentU(const MapNode& node) const override { return uValue; }
    float getCurrentV(const MapNode& node) const override { return vValue; }
    float getDepth(MapNode& node) override { return depthValue; }
    float getTemp(MapNode& node) override { return tempValue; }

    // Values to be set in tests
    float uValue = 0.0f;
    float vValue = 0.0f;
    float depthValue = 1.0f;
    float tempValue = 10.0f;

private:
    std::vector<MapNode *> empty_nodes_;
    std::vector<std::vector<float>> empty_depths_;
    std::vector<std::vector<float>> empty_temps_;
};

// Helper function to create MapNodes for testing
inline std::unique_ptr<MapNode> createMapNode(float x, float y, HabitatType type = HabitatType::Distributary) {
    auto mapNode = std::make_unique<MapNode>(type, 0.0, 0.0, 0.0);
    mapNode->x = x;
    mapNode->y = y;
    return mapNode;
}

inline std::function<float(Model&, MapNode&, float)> createMockFitnessCalculator(float returnValue) {
    return [returnValue](Model& model, MapNode& node, float cost) -> float {
        return returnValue;
    };
}

// Helper to connect two nodes with an edge
inline void connectNodes(MapNode* nodeA, MapNode* nodeB, float length) {
    Edge edge(nodeA, nodeB, length);
    nodeA->edgesOut.push_back(edge);
    nodeB->edgesIn.push_back(edge);
}

// Helper to set depth for a node in the mock hydro model
inline void setNodeDepth(MockHydroModel* hydroModel, float depth) {
    hydroModel->depthValue = depth;
}

#endif // TEST_UTILITIES_H
