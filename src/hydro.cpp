#include "hydro.h"
#include "load.h"

#include <cmath>

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

#define MIN_WT 0.01f

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
    return scalar_proj;
}

// Get the current horizontal (E/W) flow velocity in m/s at the given node
float HydroModel::getCurrentU(DistribHydroNode &hydroNode) {
    return hydroNode.us[currTimestep];
}

// Get the current vertical (N/S) flow velocity in m/s at the given node
float HydroModel::getCurrentV(DistribHydroNode &hydroNode) {
    return hydroNode.vs[currTimestep];
}

// Get the total flow velocity in m/s at the given node
float HydroModel::getFlowSpeedAt(MapNode &node) {
    if (this->useSimData) {
        return isDistributary(node.type) ? this->simDistFlow / (this->getDepth(node) * sqrt(node.area)) : 0.0f;
    }
    float currU = this->getCurrentU(this->hydroNodes[node.nearestHydroNodeID]);
    float currV = this->getCurrentV(this->hydroNodes[node.nearestHydroNodeID]);
    return sqrt(currU*currU + currV*currV);
}

// Get the current temperature (C) at the given node
float HydroModel::getTemp(MapNode &node) {
    if (this->useSimData) {
        return this->simTemps[&node][this->currTimestep];
    }
    if (isDistributary(node.type)) {
        return this->hydroNodes[node.nearestHydroNodeID].temps[currTimestep];
    }
    return fmax(MIN_WT,this->hydroNodes[node.nearestHydroNodeID].temps[currTimestep]);

    // return (float) fmax(MIN_WT, WT_predict(this->currFlowVol, this->currCresTide, node.elev, this->currAirTemp));
}

// Get the current depth (m) at the given node
// Depth is hacked to be 5m in distributary midchannel, 3m at distributary edges
// (based on blind channel model everywhere else)
float HydroModel::getDepth(MapNode &node) {
    if (this->useSimData) {
        return this->simDepths[&node][this->currTimestep];
    }
    if (isDistributary(node.type)) {
        return this->hydroNodes[node.nearestHydroNodeID].wses[currTimestep] - node.elev;
    }
    return this->hydroNodes[node.nearestHydroNodeID].wses[currTimestep] - node.elev;

    // return WSE_predict(this->currFlowVol, this->currCresTide, node.elev) - node.elev;
}