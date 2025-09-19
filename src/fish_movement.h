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
    virtual ~FishMovement() = default;

    explicit FishMovement(Model &model, float swimSpeed, float swimRange, const std::function<float(Model &, MapNode &, float)> &fitnessCalculator)
        : model(model), hydroModel(&model.hydroModel), swimSpeed(swimSpeed), swimRange(swimRange), fitnessCalculator(fitnessCalculator) {}

    double calculateTransitSpeed(const Edge &edge, const MapNode *startNode, double stillWaterSwimSpeed) const;
    virtual bool canMoveInDirectionOfEndNode(float transitSpeed, float swimSpeed) const;

    void addCurrentLocation(std::vector<std::tuple<MapNode *, float, float> > &neighbors, MapNode * point, float accumulated_cost, float stay_cost, float current_location_fitness);
    void addReachableNeighbors(std::vector<std::tuple<MapNode *, float, float>> &neighbors, MapNode * point, float accumulated_cost, MapNode * map_node) const;
    std::vector<std::tuple<MapNode *, float, float> > getReachableNeighbors(
        MapNode *startPoint,
        float accumulatedCost,
        MapNode *initialFishLocation
    ) const;

private:
    Model &model;
    HydroModel *hydroModel;
    float swimSpeed;
    float swimRange;
    const std::function<float(Model &, MapNode &, float)> fitnessCalculator;

    double calculateEffectiveSwimSpeed(const MapNode &startNode, const MapNode &endNode,
                                       double stillWaterSwimSpeed) const;

    float getCurrentU(const MapNode &node) const;
    float getCurrentV(const MapNode &node) const;
};


#endif //FISHMOVEMENT_H
