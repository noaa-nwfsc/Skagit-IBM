//
// Created by Troy Frever on 5/19/25.
//

#ifndef FISHMOVEMENT_H
#define FISHMOVEMENT_H

#include <functional>

#include "model.h"
#include "map.h"

#define MOVEMENT_DEPTH_CUTOFF 0.2f

class FishMovement {
public:
    explicit FishMovement(Model &model)
        : model(model), hydroModel(&model.hydroModel) {}

    double calculateTransitSpeed(const Edge &edge, const MapNode *startNode, double stillWaterSwimSpeed) const;

    std::vector<std::tuple<MapNode *, float, float> > getReachableNeighbors(
        MapNode *startPoint,
        float swimSpeed,
        float swimRange,
        float currentCost,
        MapNode *fishLocation,
        const std::function<float(Model &, MapNode &, float)> &fitnessCalculator
    ) const;

private:
    Model &model;
    HydroModel *hydroModel;

    double calculateEffectiveSwimSpeed(const MapNode &startNode, const MapNode &endNode,
                                       double stillWaterSwimSpeed) const;

    float getCurrentU(const MapNode &node) const;
    float getCurrentV(const MapNode &node) const;
};


#endif //FISHMOVEMENT_H
