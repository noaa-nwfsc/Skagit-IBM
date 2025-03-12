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

float habTypeMortalityConst(const HabitatType t, const float habitatMortalityMultiplier) {
    switch(t){
    case HabitatType::Distributary:
    // case HabitatType::Harbor:
    case HabitatType::Nearshore:
    // case HabitatType::Impoundment:
        return habitatMortalityMultiplier;
    default:
        return 1.0;
    }
}

Edge::Edge(MapNode *source, MapNode *target, float length)
     : source(source), target(target), length(length)
     {}

MapNode::MapNode(HabitatType type, float area, float elev, float pathDist)
        : id(-1), type(type), area(area), elev(elev), pathDist(pathDist),
        crossChannelA(nullptr), crossChannelB(nullptr), popDensity(0.0f), nearestHydroNodeID(std::numeric_limits<unsigned>::max()), hydroNodeDistance(std::numeric_limits<float>::max()) {}

SamplingSite::SamplingSite(std::string siteName, size_t id) : siteName(siteName), id(id), points() {}

float getDistance(MapNode *a, MapNode *b) {
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    return std::sqrt(dx*dx + dy*dy);
}
