#include "hydro.h"
#include "load.h"

#include <cmath>
#include <iostream>

#define WSE_intercept 0.3373725
#define WSE_flow_m3ps 0.00011386 // flow = m3/s
#define WSE_cres_tide 0.07900028 // cres tide = ft
#define WSE_elev_m 0.2723198
 // Elev = dist.d88 (NAVD88)
#define WSE_elev_x_flow 0.00020908
#define WSE_elev_x_cres_tide -0.02709448
#define WSE_flow_x_cres_tide -0.00002296

#define WT_intercept 4.091089
#define WT_flow_m3ps -0.00113428
#define WT_cres_tide 0.0602203
#define WT_elev_m 0.2295117
 // Elev = dist.d88 (NAVD88)
#define WT_elev_x_flow -0.003524042
#define WT_elev_x_cres_tide -0.03843391
#define WT_flow_x_cres_tide 0.0001238108
#define WT_air_temp_c 0.781514

#define MIN_WATER_TEMP 0.01f
#define MIN_WATER_TEMP_DISTRIBUTARY 4.0f
#define MAX_WATER_TEMP 30.0f

#define MIN_DEPTH 0.0f
#define MIN_DEPTH_DISTRIBUTARY 0.2f

// Calculate water temperature from flow (m^3/s), tide (m), node elevation (m), and air temperature (degrees C)
inline float WT_predict(float flow, float cres_tide, float elev_m, float air_temp_c) {
  return WT_intercept + WT_flow_m3ps*flow + WT_cres_tide*cres_tide + WT_elev_m*elev_m + WT_elev_x_flow*elev_m*flow + WT_elev_x_cres_tide*elev_m*cres_tide + WT_flow_x_cres_tide*flow*cres_tide + WT_air_temp_c*air_temp_c;
}

// Calculate water surface elevation from flow (m^3/s), tide (m), and node elevation (m)
inline float WSE_predict(float flow, float cres_tide, float elev_m) {
  float l = WSE_intercept + WSE_flow_m3ps*flow + WSE_cres_tide*cres_tide + WSE_elev_m*elev_m + WSE_elev_x_flow*elev_m*flow + WSE_elev_x_cres_tide*elev_m*cres_tide + WSE_flow_x_cres_tide*flow*cres_tide;
  return exp(l) - 1.0f;
}

// Initialize a hydro model from datafiles at the provided paths and a timestep offset into the data
HydroModel::HydroModel(
    std::string cresTideFilename,
    std::string flowVolFilename,
    std::string airTempFilename,
    std::string flowSpeedFilename,
    std::string distribWseTempFilename,
    int hydroTimeIntercept
) :
    cresTideData(loadFloatListInterleaved(cresTideFilename, 4)),
    flowVolData(loadFloatListInterleaved(flowVolFilename, 4)),
    airTempData(loadFloatListInterleaved(airTempFilename, 4)),
    hydroNodes(),
    useSimData(false),
    hydroTimeIntercept(hydroTimeIntercept)
{
    loadDistribHydro(flowSpeedFilename, distribWseTempFilename, this->hydroNodes);
    this->updateTime(0L);
}

HydroModel::HydroModel(
    std::vector<MapNode *> &map,
    std::vector<std::vector<float>> &depths,
    std::vector<std::vector<float>> &temps,
    float distFlow
) :
    useSimData(true), simDepths(), simTemps(), simDistFlow(distFlow)
{
    this->updateTime(0L);
    for (size_t i = 0; i < map.size(); ++i) {
        this->simDepths[map[i]] = depths[i];
        this->simTemps[map[i]] = temps[i];
    }
}

long HydroModel::getTime() const {
    return currTimestep;
}

void HydroModel::updateTime(long newTime) {
    if (!this->useSimData) {
        this->currCresTide = this->cresTideData[newTime + hydroTimeIntercept];
        this->currFlowVol = this->flowVolData[newTime + hydroTimeIntercept];
        this->currAirTemp = this->airTempData[newTime + hydroTimeIntercept];
    }
    this->currTimestep = newTime;
}

bool HydroModel::isHighTide() {
    return this->currTimestep + hydroTimeIntercept - 1 > 0 && this->currTimestep + hydroTimeIntercept + 1 < (long) this->cresTideData.size()
        && this->currCresTide > this->cresTideData[this->currTimestep + hydroTimeIntercept - 1]
        && this->currCresTide > this->cresTideData[this->currTimestep + hydroTimeIntercept + 1];
}

// Calculate flow speed along the provided edge
float HydroModel::getFlowSpeedAlong(Edge &edge) {
    if (this->useSimData) {
        return isDistributary(edge.source->type) ? this->simDistFlow : 0.0f;
    }
    // Get the current flow vector at the edge's source
    float u = this->getCurrentU(this->hydroNodes[edge.source->nearestHydroNodeID]);
    float v = this->getCurrentV(this->hydroNodes[edge.source->nearestHydroNodeID]);
    // Get the edge's vector representation
    float dx = edge.target->x - edge.source->x;
    float dy = edge.target->y - edge.source->y;
    // Calculate how colinear the flow vector and the given edge are
    float d = sqrt(dx * dx + dy * dy);
    float scalar_proj = u*(dx/d) + v*(dy/d);
    return scaledFlowSpeed(scalar_proj, *(edge.source));
}

// Get the current horizontal (E/W) flow velocity in m/s at the given node
float HydroModel::getCurrentU(const MapNode &node) const {
    return this->getCurrentU(this->hydroNodes[node.nearestHydroNodeID]);
}
float HydroModel::getCurrentU(const DistribHydroNode &hydroNode) const {
    return hydroNode.us[currTimestep];
}

// Get the current vertical (N/S) flow velocity in m/s at the given node
float HydroModel::getCurrentV(const MapNode &node) const {
    return this->getCurrentV(this->hydroNodes[node.nearestHydroNodeID]);
}
float HydroModel::getCurrentV(const DistribHydroNode &hydroNode) const {
    return hydroNode.vs[currTimestep];
}

// Get the total flow velocity in m/s at the given node
float HydroModel::getUnsignedFlowSpeedAtHydroNode(DistribHydroNode &hydroNode) {
    float currU = this->getCurrentU(hydroNode);
    float currV = this->getCurrentV(hydroNode);
    return sqrt(currU*currU + currV*currV);
}

FlowVelocity HydroModel::getScaledFlowVelocityAt(MapNode &node) {
    auto scalar = static_cast<float>(calculateFlowSpeedScalar(node));
    return {getCurrentU(node) * scalar, getCurrentV(node) * scalar};
}

double HydroModel::calculateFlowSpeedScalar(const MapNode &node) {
    if (!isBlindChannel(node.type) && !isImpoundment(node.type)) {
        return 1.0;
    }
    const double hydroFlowSpeed = this->getUnsignedFlowSpeedAtHydroNode(this->hydroNodes[node.nearestHydroNodeID]);
    const double hydroWidth = pow((hydroFlowSpeed / 0.04479583), (1.0 / 0.45896));
    const double blindChannelWidth = sqrt(node.area);
    double scalar = blindChannelWidth / hydroWidth;

    if (scalar > 1.0) {
        scalar = 1.0;
    }
    if (isImpoundment(node.type)) {
        constexpr double IMPOUNDMENT_MIN_FLOW_ADDL_SCALAR = 0.1;
        scalar *= IMPOUNDMENT_MIN_FLOW_ADDL_SCALAR;
    }
    return scalar;
}

float HydroModel::scaledFlowSpeed(const float flowSpeed, const MapNode &node) {
    double scalar = calculateFlowSpeedScalar(node);
    const double scaledFlowSpeed = scalar * flowSpeed;
    return static_cast<float>(scaledFlowSpeed);
}


float HydroModel::getUnsignedFlowSpeedAt(MapNode &node) {
    if (this->useSimData) {
        return isDistributary(node.type) ? this->simDistFlow / (this->getDepth(node) * sqrt(node.area)) : 0.0f;
    }
    const float velocity = this->getUnsignedFlowSpeedAtHydroNode(this->hydroNodes[node.nearestHydroNodeID]);
    return scaledFlowSpeed(velocity, node);
}

float limitWaterTemp(float waterTemp, HabitatType nodeType) {
    float waterTemperature = waterTemp;
    if (waterTemperature > MAX_WATER_TEMP) {
        waterTemperature = MAX_WATER_TEMP;
    }
    const float minimum_water_temperature = isDistributaryOrHarbor(nodeType) ? MIN_WATER_TEMP_DISTRIBUTARY : MIN_WATER_TEMP;
    if (waterTemperature < minimum_water_temperature) {
        waterTemperature = minimum_water_temperature;
    }

    return waterTemperature;
}

// Get the current temperature (C) at the given node
float HydroModel::getTemp(MapNode &node) {
    if (this->useSimData) {
        return this->simTemps[&node][this->currTimestep];
    }

    const float hydroTemp = this->hydroNodes[node.nearestHydroNodeID].temps[currTimestep];
    return limitWaterTemp(hydroTemp, node.type);
}

float limitDepth(const float depth, const HabitatType nodeType) {
    const float min_depth = isDistributaryOrHarbor(nodeType) ? MIN_DEPTH_DISTRIBUTARY : MIN_DEPTH;
    return (depth < min_depth) ? min_depth : depth;
}

// Get the current depth (m) at the given node
// Depth is hacked to be 5m in distributary midchannel, 3m at distributary edges
// (based on blind channel model everywhere else)
float HydroModel::getDepth(MapNode &node) {
    if (this->useSimData) {
        return this->simDepths[&node][this->currTimestep];
    }

    const float depth = this->hydroNodes[node.nearestHydroNodeID].wses[currTimestep] - node.elev;
    return limitDepth(depth, node.type);
}