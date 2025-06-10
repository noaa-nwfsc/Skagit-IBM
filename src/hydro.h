#ifndef __FISH_HYDRO_H
#define __FISH_HYDRO_H

#include <vector>
#include <unordered_map>
#include "map.h"

// This struct stores cached hydrology model predictions for a single map location
typedef struct HydroNode {
    float temp; // temperature in degrees C
    float depth; // meters of water (distance from map elevation to water surface elevation)
    float flowSpeed; // flow speed in m/s
} HydroNode;

class HydroModel {
public:
    HydroModel(
        std::string cresTideFilename,
        std::string flowVolFilename,
        std::string airTempFilename,
        std::string flowSpeedFilename,
        std::string distribWseTempFilename,
        int hydroTimeIntercept // Timesteps between midnight on Jan 1 and the start of the cresTide, flowVol, and airTemp data
    );

    HydroModel(
        std::vector<MapNode *> &map,
        std::vector<std::vector<float>> &depths,
        std::vector<std::vector<float>> &temps,
        float distFlow
    );

    virtual ~HydroModel() = default;

    // Return the flow speed in m/s along the provided edge (from edge.source to edge.target)
    float getFlowSpeedAlong(Edge &edge);
    // Return the flow speed in m/s at a given location
    float getUnsignedFlowSpeedAt(MapNode &node);
    float getUnsignedFlowSpeedAtHydroNode(DistribHydroNode &hydroNode);
    FlowVelocity getScaledFlowVelocityAt(MapNode &node);

    double calculateFlowSpeedScalar(const MapNode &node);

    virtual float scaledFlowSpeed(float flowSpeed, const MapNode &node);
    // Return the temperature in degrees C at a given location
    float getTemp(MapNode &node);
    // Return the water depth in meters at a given location
    float getDepth(MapNode &node);
    // Check if the current timestep is a high tide
    bool isHighTide();

    // Set the hydro model's timestep to a given timestep
    void updateTime(long newTime);

    long getTime() const;

public:
    virtual float getCurrentU(const MapNode& node) const; // m/s
    virtual float getCurrentV(const MapNode& node) const; // m/s
protected:
    float getCurrentU(const DistribHydroNode &hydroNode) const; // Get the current timestep's horizontal flow speed component (in m/s) at a given DistribHydroNode
    float getCurrentV(const DistribHydroNode &hydroNode) const; // Get the current timestep's vertical flow speed component (in m/s) at a given DistribHydroNode

public:
    // The loaded crescent tide data, in m
    std::vector<float> cresTideData;
    // The loaded flow volume data, in m^3/s
    std::vector<float> flowVolData;
    // The loaded air temperature data, in degrees C
    std::vector<float> airTempData;
    // The loaded flow data, as DistribHydroNodes (see map.h)
    std::vector<DistribHydroNode> hydroNodes;

private:
    bool useSimData;
    std::unordered_map<MapNode *, std::vector<float>> simDepths;
    std::unordered_map<MapNode *, std::vector<float>> simTemps;
    float simDistFlow;

    int hydroTimeIntercept;
    float currCresTide;
    float currFlowVol;
    float currAirTemp;
    long currTimestep;

};

#endif