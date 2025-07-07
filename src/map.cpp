#include "map.h"
#include <vector>
#include <string>
#include <cmath>
#include <limits>

bool isDistributary(HabitatType t) {
    switch(t){
    case HabitatType::Distributary:
    case HabitatType::DistributaryEdge:
        return true;
    default:
        return false;
    }
}

bool isHarbor(HabitatType t) {
    return t == HabitatType::Harbor;
}

bool isNearshore(HabitatType t) {
    return t == HabitatType::Nearshore;
}

bool isBlindChannel(HabitatType t) {
    return t == HabitatType::BlindChannel;
}

bool isImpoundment(HabitatType t) {
    return t == HabitatType::Impoundment;
}
bool isDistributaryOrHarbor(const HabitatType t) {
    return isDistributary(t) || isHarbor(t);
}

bool isDistributaryOrNearshore(const HabitatType t) {
    return isDistributary(t) || isNearshore(t);
}

bool isDistributaryExcludingDistributaryEdgeOrNearshore(HabitatType habitat) {
    return habitat == HabitatType::Distributary || isNearshore(habitat);
}

float habitatTypeMortalityConst(const HabitatType t, const float habitatMortalityMultiplier) {
    float defaultNoMultiplier = 1.0;
    if (isDistributaryExcludingDistributaryEdgeOrNearshore(t)) {
        return habitatMortalityMultiplier;
    }
    return defaultNoMultiplier;
}

Edge::Edge(MapNode *source, MapNode *target, float length)
     : source(source), target(target), length(length)
     {}

MapNode::MapNode(HabitatType type, float area, float elev, float pathDist)
        : id(-1), type(type), area(area), elev(elev), pathDist(pathDist),
        crossChannelA(nullptr), crossChannelB(nullptr),
        nearestHydroNodeID(std::numeric_limits<unsigned>::max()), hydroNodeDistance(std::numeric_limits<float>::max()),
        popDensity(0.0f)
{}

SamplingSite::SamplingSite(std::string siteName, size_t id) : siteName(siteName), id(id), points() {}

float getDistance(MapNode *a, MapNode *b) {
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    return std::sqrt(dx*dx + dy*dy);
}
