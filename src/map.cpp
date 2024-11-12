#include "map.h"
#include <vector>
#include <string>
#include <cmath>

bool isDistributary(HabitatType t) {
    switch(t){
    case HabitatType::Distributary:
    case HabitatType::DistributaryEdge:
        return true;
    default:
        return false;
    }
}

bool isDistributaryOrHarbor(const HabitatType t) {
    switch(t) {
    case HabitatType::Distributary:
    case HabitatType::DistributaryEdge:
        return true;
    default:
        return false;
    }
}

bool isNearshore(HabitatType t) {
    switch(t){
    case HabitatType::Nearshore:
        return true;
    default:
        return false;
    }
}

float habTypeMortalityConst(HabitatType t) {
    switch(t){
    case HabitatType::Distributary:
    // case HabitatType::Harbor:
    case HabitatType::Nearshore:
    // case HabitatType::Impoundment:
        return 2.0;
    default:
        return 1.0;
    }
}

Edge::Edge(MapNode *source, MapNode *target, float length)
     : source(source), target(target), length(length)
     {}

MapNode::MapNode(HabitatType type, float area, float elev, float pathDist)
        : id(-1), type(type), area(area), elev(elev), pathDist(pathDist),
        crossChannelA(nullptr), crossChannelB(nullptr), popDensity(0.0f) {}

SamplingSite::SamplingSite(std::string siteName, size_t id) : siteName(siteName), id(id), points() {}

float getDistance(MapNode *a, MapNode *b) {
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    return std::sqrt(dx*dx + dy*dy);
}
