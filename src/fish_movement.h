//
// Created by Troy Frever on 5/19/25.
//

#ifndef FISHMOVEMENT_H
#define FISHMOVEMENT_H

#include "hydro.h"
#include "map.h"

class FishMovement {
public:
    explicit FishMovement(HydroModel *hydro_model)
        : hydroModel(hydro_model) {
    }

    double calculateTransitSpeed(const Edge& edge, const MapNode* startNode, double stillWaterSwimSpeed) const;

private:
    HydroModel* hydroModel;

    double calculateEffectiveSwimSpeed(const MapNode& startNode, const MapNode& endNode, double stillWaterSwimSpeed) const;

    float getCurrentU(const MapNode& node) const;
    float getCurrentV(const MapNode& node) const;
};



#endif //FISHMOVEMENT_H
