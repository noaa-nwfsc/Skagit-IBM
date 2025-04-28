#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <queue>
#include <tuple>
#include <algorithm>
#include <netcdf>

#include "load.h"
#include "load_utils.h"
#include "hydro.h"

// calculate distance between <x1, y1> and <x2, y2>
inline float distance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return sqrt(dx*dx + dy*dy);
}

// Load the distributary hydrology data from two NetCDF files
// the "nodesOut" argument is an output
// After this method is called, it will contain a list of
// "DistribHydroNode" objects, each of which has a 2d position and a list of hourly flow vectors,
// water surface elevations, and water temperatures
void loadDistribHydro(std::string &flowPath, std::string &wseTempPath, std::vector<DistribHydroNode> &nodesOut) {
    netCDF::NcFile flowSourceFile(flowPath, netCDF::NcFile::FileMode::read);
    netCDF::NcFile wseTempSourceFile(wseTempPath, netCDF::NcFile::FileMode::read);
    size_t nodeCount = flowSourceFile.getDim("node").getSize();
    size_t timeCount = flowSourceFile.getDim("time").getSize();
    netCDF::NcVar x = flowSourceFile.getVar("x");
    netCDF::NcVar y = flowSourceFile.getVar("y");
    netCDF::NcVar u = flowSourceFile.getVar("u");
    netCDF::NcVar v = flowSourceFile.getVar("v");
    netCDF::NcVar wse = wseTempSourceFile.getVar("wse");
    netCDF::NcVar temp = wseTempSourceFile.getVar("temp");
    // Create each node
    std::cout << std::endl;
    for (size_t i = 0; i < nodeCount; ++i) {
        std::cout << "\rloading distributary hydrology data: " << (i+1) << "/" << nodeCount;
        std::cout.flush();
        nodesOut.emplace_back(i);
        DistribHydroNode &node = nodesOut.back();
        std::vector<size_t> coordIndex{i};
        x.getVar(coordIndex, &node.x);
        y.getVar(coordIndex, &node.y);
        // These vectors are passed as indices to getVar to retrieve the NetCDF data
        std::vector<size_t> flowIndex{0, i};
        std::vector<size_t> flowCounts{timeCount, 1};
        // resize the lists' memory footprint ahead of time to fit the right number of timesteps
        node.us.resize(timeCount);
        node.vs.resize(timeCount);
        node.wses.resize(timeCount);
        node.temps.resize(timeCount);
        // Retrieve the data from the NetCDF file, store it in the node's flow component lists
        u.getVar(flowIndex, flowCounts, node.us.data());
        v.getVar(flowIndex, flowCounts, node.vs.data());
        wse.getVar(flowIndex, flowCounts, node.wses.data());
        temp.getVar(flowIndex, flowCounts, node.temps.data());

        fix_all_missing_values(timeCount, NetCDFVarVectorAdapter(u), node.us);
        fix_all_missing_values(timeCount, NetCDFVarVectorAdapter(v), node.vs);
        fix_all_missing_values(timeCount, NetCDFVarVectorAdapter(wse), node.wses);
        fix_all_missing_values(timeCount, NetCDFVarVectorAdapter(temp), node.temps);
    }
    // Log that we've finished loading
    std::cout << std::endl << "done loading hydro" << std::endl;
}

void assignHydroNodeToMapNodeWithDistance(const unsigned hydroNodeIndex, MapNode *mapNode, const float distance) {
    mapNode->nearestHydroNodeID = hydroNodeIndex;
    mapNode->hydroNodeDistance = distance;
}

void initializeEachHydroNodeToNearestMapNode(const std::vector<MapNode *> & map, const std::vector<DistribHydroNode> & hydroNodes, std::unordered_set<MapNode*>& assignedNodes) {
    for (unsigned hydroNodeIndex = 0; hydroNodeIndex < hydroNodes.size(); ++hydroNodeIndex) {
        MapNode* closestNode = nullptr;
        float closestDistance = std::numeric_limits<float>::max();
        for (MapNode *node : map) {
            if (!isDistributaryOrNearshore(node->type)) {
                continue;;
            }
            const float nodeDistance = distance(hydroNodes[hydroNodeIndex].x, hydroNodes[hydroNodeIndex].y, node->x, node->y);
            if (nodeDistance < closestDistance) {
                closestDistance = nodeDistance;
                closestNode = node;
            }
        }
        if (closestDistance < closestNode->hydroNodeDistance) {
            assignHydroNodeToMapNodeWithDistance(hydroNodeIndex, closestNode, closestNode->hydroNodeDistance);
            assignedNodes.emplace(closestNode);
        }
    }
    for (auto assignedNode : assignedNodes) {
        assignedNode->hydroNodeDistance = 0;
    }
}

class MinPriorityTupleComparator {
public:
    bool operator()(const std::tuple<float, MapNode*>& lhs,
                const std::tuple<float, MapNode*>& rhs) const {
        return std::get<0>(lhs) > std::get<0>(rhs);
    }
};


using DijkstraMinQueue = std::priority_queue<
    std::tuple<float, MapNode*>,
    std::vector<std::tuple<float, MapNode*>>,
    MinPriorityTupleComparator
>;

void initializeDijkstraQueue(DijkstraMinQueue & dijkstra_queue, const std::unordered_set<MapNode *> & initialNodes) {
    for (MapNode *node : initialNodes) {
        dijkstra_queue.emplace(0, node);
    }
}

void assignRemainingMapNodesToHydroNodes(DijkstraMinQueue & dijkstraMinQueue) {
    while (! dijkstraMinQueue.empty()) {
        auto queueTuple = dijkstraMinQueue.top();
        dijkstraMinQueue.pop();
        const float distance = std::get<0>(queueTuple);
        MapNode *node = std::get<1>(queueTuple);

        if (distance > node->hydroNodeDistance) {
            continue;
        }

        std::vector<std::tuple<MapNode *, float>> neighbors;
        neighbors.reserve( node->edgesIn.size() + node->edgesOut.size() );
        for (Edge &edge : node->edgesIn) {
            neighbors.emplace_back(edge.source, edge.length);
        }
        for (Edge &edge : node->edgesOut) {
            neighbors.emplace_back(edge.target, edge.length);
        }

        for (auto neighbor : neighbors) {
            MapNode* neighborNode = std::get<0>(neighbor);
            float edgeLength = std::get<1>(neighbor);
            float neighborDistance = distance + edgeLength;
            if (neighborDistance < neighborNode->hydroNodeDistance) {
                neighborNode->hydroNodeDistance = neighborDistance;
                neighborNode->nearestHydroNodeID = node->nearestHydroNodeID;
                dijkstraMinQueue.emplace(neighborDistance, neighborNode);
            }
        }
    }
}

// Identify single closest map node for each hydro node, and assign the hydro node to that map node. For each remaining
// map nodes, use the Dijkstra algorithm to calculate the shortest total edge distance to one of the initially
// assigned nodes, and copy its hydro node association

void assignNearestHydroNodesByEdgeDistance(std::vector<MapNode *> &map, const std::vector<DistribHydroNode> &hydroNodes) {
    auto initialNodes = std::unordered_set<MapNode*>();
    initializeEachHydroNodeToNearestMapNode(map, hydroNodes, initialNodes);
    DijkstraMinQueue dijkstraMinQueue{MinPriorityTupleComparator()};
    initializeDijkstraQueue(dijkstraMinQueue, initialNodes);
    assignRemainingMapNodesToHydroNodes(dijkstraMinQueue);
}

void assignNearestHydroNodes(std::vector<MapNode *> &map, std::vector<DistribHydroNode> &hydroNodes) {
    assignNearestHydroNodesByEdgeDistance(map, hydroNodes);
    return;

    // Set the nearestHydroNodeID field for each node in "map" by finding its nearest
    // HydroNode in "hydroNodes"
    for (MapNode *node : map) {
        node->nearestHydroNodeID = 0;
        float bestDist = distance(hydroNodes[0].x, hydroNodes[0].y, node->x, node->y);
        for (unsigned id = 1; id < hydroNodes.size(); ++id) {
            float dist = distance(hydroNodes[id].x, hydroNodes[id].y, node->x, node->y);
            if (dist < bestDist) {
                bestDist = dist;
                node->nearestHydroNodeID = id;
            }
        }
    }
}

// Adjust the map's elevation values to make the minimum depth in distributary channels
// at least a cutoff value (20cm)
void fixElevations(std::vector<MapNode *> &map, std::vector<DistribHydroNode> &hydroNodes) {
    const float cutoffDepth = 0.2f;
    float minDistribDepth = cutoffDepth;
    for (MapNode *node : map) {
        if (isDistributary(node->type)) {
            for (float wse : hydroNodes[node->nearestHydroNodeID].wses) {
                float depth = wse - node->elev;
                if (depth < minDistribDepth) {
                    minDistribDepth = depth;
                }
            }
        }
    }
    float correction = cutoffDepth - minDistribDepth;
    for (MapNode *node : map) {
        node->elev -= correction;
    }
}

// Split a string into a list of strings delimited by the character given in argument "c"
std::vector<std::string> split(std::string &s, char c) {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string line;
    // Get the next block of characters terminated by c, until none are left (getline returns 0 when this is the case)
    while (std::getline(ss, line, c)) {
        // Add the block to the result list
        result.push_back(line);
    }
    return result;
}

// Load a recruit size distribution array from a CSV
void loadRecSizeDists(std::string &filePath, std::vector<std::vector<float>> &out) {
    std::ifstream f;
    f.open(filePath);
    std::string line;
    bool first = true;
    // Get lines from the file until it's empty
    while (std::getline(f, line)) {
        if (first) {
            // Skip the first line since it's a header with field names
            first = false;
            continue;
        }
        // Make sure the line isn't empty
        if (line.size() > 0) {
            // Put a new list (to hold this row) on the output list
            out.emplace_back();
            // Get a reference to it
            std::vector<float> &dist = out.back();
            // Convert each comma-separated field into a float
            for (std::string chunk : split(line, ',')) {
                dist.push_back(std::stof(chunk));
            }
        }
    }
}

// Load a list of integers from a file (where each integer is on its own line)
// The argument "out" is where the results will be stored
void loadIntList(std::string &filePath, std::vector<int> &out) {
    std::ifstream f;
    f.open(filePath);
    std::string line;
    // Get lines from the file until it's empty
    while (std::getline(f, line)) {
        // For each line
        // Make sure the line isn't empty before trying to parse an int
        if (line.size() > 0)
            out.push_back(std::stoi(line));
    }
}

// Alias for loadIntList that creates a new list to store the results
std::vector<int> loadIntList(std::string &filePath) {
    std::vector<int> result;
    loadIntList(filePath, result);
    return result;
}

// Load a list of floats from a file (where each float is on its own line)
// The argument "out" is where the results will be stored
void loadFloatList(std::string &filePath, std::vector<float> &out) {
    std::ifstream f;
    f.open(filePath);
    std::string line;
    // Get lines from the file until it's empty
    while (std::getline(f, line)) {
        // For each line
        // Make sure the line isn't empty before trying to parse a float
        if (line.size() > 0)
            out.push_back(std::stof(line));
    }
}

// Load every nth float from a list of floats
std::vector<float> loadFloatListInterleaved(std::string &filePath, int n) {
    std::vector<float> result;
    std::ifstream f;
    f.open(filePath);
    std::string line;
    int i = 0;
    while (std::getline(f, line)) {
        if (line.size() > 0) {
            if (i % n == 0) {
                result.push_back(std::stof(line));
            }
            ++i;
        }
    }
    return result;
}

// Alias for loadFloatList that creates a new list to store the results in
std::vector<float> loadFloatList(std::string &filePath) {
    std::vector<float> result;
    loadFloatList(filePath, result);
    return result;
}

// Intermediate datatype to hold the node information loaded from a map vertex CSV
typedef struct MapNodeData {
    unsigned id;
    float area;
    float elev;
    float pathDist;
    HabitatType type;
    MapNodeData(unsigned id, float area, float pathDist, float elev, HabitatType type)
     : id(id), area(area), elev(elev), pathDist(pathDist), type(type) {}
} MapNodeData;

// Map from string identifiers to their HabitatType enum values
const std::unordered_map<std::string, HabitatType> habTypeByName {
    {"blind channel", HabitatType::BlindChannel},
    {"impoundment", HabitatType::Impoundment},
    {"low tide terrace", HabitatType::LowTideTerrace},
    {"distributary channel", HabitatType::Distributary},
    {"boat harbor", HabitatType::Harbor},
    {"nearshore", HabitatType::Nearshore},
    {"shoreline", HabitatType::Nearshore}
};

// Load sampling site structs from a sampling site CSV
// In order to do this, we have to figure out what the closest map nodes
// to link to the site are, since sites are defined by coordinates that likely aren't overlapping a node
/*
void loadSamplingSites(std::string &filePath, std::vector<MapNode *> &map, std::vector<SamplingSite> &out) {
    std::ifstream file;
    file.open(filePath);
    std::string line;
    bool first = true;
    size_t id = 0;
    while (std::getline(file, line)) {
        if (first) {
            // Skip the first line since it's a header with field names
            first = false;
            continue;
        }
        std::vector<std::string> chunks = split(line, ',');
        // Parse the coordinates from the CSV row
        float siteX = std::stof(chunks[0]);
        float siteY = std::stof(chunks[1]);
        // Create the sampling site struct from the position, name and ID fields in the CSV row
        out.emplace_back(siteX, siteY, chunks[2], id);
        ++id;
        // Get a reference to it
        SamplingSite &site = out.back();
        // Find all nodes within 20 meters of the site (and find the closest node while we're at it)
        MapNode *closest = map[0];
        float minDistance = distance(closest->x, closest->y, siteX, siteY);
        for (MapNode *node : map) {
            float currDistance = distance(node->x, node->y, siteX, siteY);
            if (currDistance < minDistance) {
                closest = node;
                minDistance = currDistance;
            }
        }
        site.points.push_back(closest);
        std::unordered_set<MapNode *> visited;
        std::vector<MapNode *> fringe;
        fringe.push_back(closest);
        while (fringe.size() > 0) {
            MapNode *curr = fringe.back();
            fringe.pop_back();
            if (!visited.count(curr)) {
                visited.insert(curr);
                site.points.push_back(curr);
                for (Edge &e : curr->edgesOut) {
                    if (!isDistributary(e.target->type) && e.target->type != HabitatType::Nearshore && e.target->pathDist > curr->pathDist) {
                        fringe.push_back(e.target);
                    }
                }
                for (Edge &e : curr->edgesIn) {
                    if (!isDistributary(e.source->type) && e.source->type != HabitatType::Nearshore && e.source->pathDist > curr->pathDist) {
                        fringe.push_back(e.source);
                    }
                }
            }
        }
    }
}
*/

// Remove an edge connecting a given node to a given neighbor
// from that node's edge lists
void removeAllEdgesBetween(MapNode *node, MapNode *neighbor) {
    // Check  out edges
    // Continue to prune edges until none connecting to the target are found
    bool modified = true;
    while (modified) {
        modified = false;
        for (auto it = node->edgesOut.begin(); it != node->edgesOut.end(); ++it) {
            if (it->target == neighbor) {
                node->edgesOut.erase(it);
                modified = true;
                break;
            }
        }
    }
    // Check in edges
    // Continue to prune edges until none connecting to the target are found
    modified = true;
    while (modified) {
        modified = false;
        for (auto it = node->edgesIn.begin(); it != node->edgesIn.end(); ++it) {
            if (it->source == neighbor) {
                node->edgesIn.erase(it);
                modified = true;
                break;
            }
        }
    }
}

// Combine two nodes
MapNode *mergeNodes(MapNode *a, MapNode *b) {
    // Construct a new node to replace them
    MapNode *newNode = new MapNode(a->type, a->area + b->area, (a->elev + b->elev)*0.5f, (a->pathDist + b->pathDist)*0.5f);
    newNode->id = a->id;
    // Place it at the average location of the two merged nodes
    newNode->x = (a->x + b->x) / 2.0f;
    newNode->y = (a->y + b->y) / 2.0f;
    // Extra length to add to edges (distance between either a or b and the new node)
    float extraLength = sqrt((a->x - b->x)*(a->x - b->x) + (a->y - b->y)*(a->y - b->y))*0.5f;
    // Create updated edges and remove existing edges held by other nodes that pointed to the
    // old nodes
    for (Edge e : a->edgesOut) {
        if (e.target != b) {
            removeAllEdgesBetween(e.target, a);
            e.target->edgesIn.emplace_back(newNode, e.target, e.length + extraLength);
            newNode->edgesOut.emplace_back(newNode, e.target, e.length + extraLength);
        }
    }
    for (Edge e : a->edgesIn) {
        if (e.source != b) {
            removeAllEdgesBetween(e.source, a);
            e.source->edgesOut.emplace_back(e.source, newNode, e.length + extraLength);
            newNode->edgesIn.emplace_back(e.source, newNode, e.length + extraLength);
        }
    }
    for (Edge e : b->edgesOut) {
        if (e.target != a) {
            removeAllEdgesBetween(e.target, b);
            e.target->edgesIn.emplace_back(newNode, e.target, e.length + extraLength);
            newNode->edgesOut.emplace_back(newNode, e.target, e.length + extraLength);
        }
    }
    for (Edge e : b->edgesIn) {
        if (e.source != a) {
            removeAllEdgesBetween(e.source, b);
            e.source->edgesOut.emplace_back(e.source, newNode, e.length + extraLength);
            newNode->edgesIn.emplace_back(e.source, newNode, e.length + extraLength);
        }
    }
    return newNode;
}

// Merge nodes that are within a certain radius of each other
void simplifyBlindChannels(std::vector<MapNode *> &map, float radius, const std::unordered_set<MapNode *> &protectedNodes) {
    std::unordered_set<MapNode *> toRemove;
    std::unordered_set<MapNode *> toAdd;
    for (MapNode *node : map) {
        // Avoid checking for merges with nodes that will already be merged
        // Only merge blind channel nodes
        // Don't touch sampling sites
        if (toRemove.count(node)
            || node->type != HabitatType::BlindChannel
            || protectedNodes.count(node)) {
            continue;
        }
        // Check outbound edges
        bool merged = false;
        for (Edge e : node->edgesOut) {
            // Don't merge nodes that are already involved in merges or protected nodes
            if (toAdd.count(e.target) || protectedNodes.count(e.target)) {
                continue;
            }
            if (e.target->type == HabitatType::BlindChannel && e.length <= radius) {
                // Tests passed, merge current node with current edge's endpoint
                toAdd.insert(mergeNodes(node, e.target));
                toRemove.insert(node);
                toRemove.insert(e.target);
                merged = true;
                // Stop checking
                break;
            }
        }
        // Don't bother to check inbound edges if a merge has already been carried out
        if (merged) {
            continue;
        }
        // Check inbound edges
        for (Edge e : node->edgesIn) {
            // Don't merge nodes that are already involved in merges or protected nodes
            if (toAdd.count(e.source) || protectedNodes.count(e.source)) {
                continue;
            }
            if (e.source->type == HabitatType::BlindChannel && e.length <= radius) {
                // Tests passed, merge current node with current edge's endpoint
                toAdd.insert(mergeNodes(node, e.source));
                toRemove.insert(node);
                toRemove.insert(e.source);
                merged = true;
                // Stop checking
                break;
            }
        }
    }
    // Condense map list to remove old nodes
    // targetIt tracks where in the list to write "good" nodes to (nodes that are still in use)
    auto targetIt = map.begin();
    // sourceIt tracks where in the list we're examining
    for (auto sourceIt = map.begin(); sourceIt != map.end(); ++sourceIt) {
        // Check if the node we're looking at in the list is still good
        if (toRemove.count(*sourceIt)) {
            // It should be deleted
            // Free its memory and wipe it from the list
            delete *sourceIt;
            *sourceIt = nullptr;
        } else {
            // It should be kept
            // Shift it over into the target position and update the target position
            *targetIt = *sourceIt;
            ++targetIt;
        }
    }
    // Chop off the unused space at the end of the list
    if (targetIt != map.end()) {
        map.erase(targetIt, map.end());
    }
    // Add the new nodes to the list
    for (MapNode *newNode : toAdd) {
        map.push_back(newNode);
    }
}

// Create a new node at the halfway point of an edge and link it to the edge's endpoints
MapNode *elaborateEdge(Edge e) {
    removeAllEdgesBetween(e.source, e.target);
    removeAllEdgesBetween(e.target, e.source);
    float areaFromSource = e.source->area * 0.25f;
    float areaFromTarget = e.target->area * 0.25f;
    float newArea = areaFromSource + areaFromTarget;
    e.source->area -= areaFromSource;
    e.target->area -= areaFromTarget;
    MapNode *newNode = new MapNode(e.target->type, newArea, (e.source->elev + e.target->elev)*0.5f, (e.source->pathDist + e.target->pathDist)*0.5f);
    newNode->x = (e.source->x + e.target->x) * 0.5f;
    newNode->y = (e.source->y + e.target->y) * 0.5f;
    e.source->edgesOut.emplace_back(e.source, newNode, e.length/2.0f);
    newNode->edgesIn.emplace_back(e.source, newNode, e.length/2.0f);
    e.target->edgesIn.emplace_back(newNode, e.target, e.length/2.0f);
    newNode->edgesOut.emplace_back(newNode, e.target, e.length/2.0f);
    return newNode;
}

// Elaborate all nearshore edges
void expandNearshoreLinks(std::vector<MapNode *> &map, unsigned int maxRealID) {
    std::unordered_set<MapNode *> toAdd;
    for (MapNode *node : map) {
        bool updated = true;
        while (updated) {
            updated = false;
            for (Edge &e : node->edgesOut) {
                if (((e.target->type == HabitatType::Nearshore) != (node->type == HabitatType::Nearshore)) && !toAdd.count(e.target)) {
                    toAdd.insert(elaborateEdge(e));
                    updated = true;
                    break;
                }
            }
        }
    }
    unsigned int newID = maxRealID + 1;
    for (MapNode *newNode : toAdd) {
        newNode->id = newID;
        map.push_back(newNode);
        ++newID;
    }
}

// Count all neighbors of a given node that are distributary nodes
unsigned countAdjacentDistributaryNodes(MapNode &node) {
    unsigned count = 0;
    for (Edge &e : node.edgesIn) {
        if (isDistributary(e.source->type)) {
            ++count;
        }
    }
    for (Edge &e : node.edgesOut) {
        if (isDistributary(e.target->type)) {
            ++count;
        }
    }
    return count;
}

// Shorthand for the difference in path distance between two nodes
inline float deltaPathDist(MapNode &node1, MapNode &node2) {
    return fabs(node1.pathDist - node2.pathDist);
}


// Figures out which nodes are channel edges and makes sure that every node in a channel
// knows which other nodes are in its "slice" of the channel
// OBSOLETED by new flow data
/*
// Early declaration for a helper method used in assignCrossChannelEdgesFromChannelCenter
void assignCrossChannelEdgesFromNode(MapNode &node);
void assignCrossChannelEdgesFromChannelCenter(MapNode &centerNode) {
    float minDeltaPathDist = 0.0f;
    Edge *bestCandidate = nullptr;
    float minDeltaPathDist2 = 0.0f;
    Edge *bestCandidate2 = nullptr;
    for (Edge &e : centerNode.edgesIn) {
        if (!isDistributary(e.source->type))
            continue;
        float currDeltaPathDist = deltaPathDist(centerNode, *e.source);
        if (currDeltaPathDist < minDeltaPathDist || bestCandidate == nullptr) {
            if (minDeltaPathDist < minDeltaPathDist2 || bestCandidate2 == nullptr) {
                bestCandidate2 = bestCandidate;
                minDeltaPathDist2 = minDeltaPathDist;
            }
            bestCandidate = &e;
            minDeltaPathDist = currDeltaPathDist;
        } else if (currDeltaPathDist < minDeltaPathDist2 || bestCandidate2 == nullptr) {
            bestCandidate2 = &e;
            minDeltaPathDist2 = currDeltaPathDist;
        }
    }
    for (Edge &e : centerNode.edgesOut) {
        if (!isDistributary(e.target->type))
            continue;
        float currDeltaPathDist = deltaPathDist(centerNode, *e.target);
        if (currDeltaPathDist < minDeltaPathDist || bestCandidate == nullptr) {
            if (minDeltaPathDist < minDeltaPathDist2 || bestCandidate2 == nullptr) {
                bestCandidate2 = bestCandidate;
                minDeltaPathDist2 = minDeltaPathDist;
            }
            bestCandidate = &e;
            minDeltaPathDist = currDeltaPathDist;
        } else if (currDeltaPathDist < minDeltaPathDist2 || bestCandidate2 == nullptr) {
            bestCandidate2 = &e;
            minDeltaPathDist2 = currDeltaPathDist;
        }
    }
    centerNode.crossChannelA = bestCandidate;
    centerNode.crossChannelB = bestCandidate2;
}

void assignCrossChannelEdgesFromChannelEdge(MapNode &edgeNode) {
    float minDeltaPathDist = 0.0f;
    Edge *bestCandidate = nullptr;
    MapNode *bestCandidateNeighbor = nullptr;
    for (Edge &e : edgeNode.edgesIn) {
        if (!isDistributary(e.source->type))
            continue;
        float currDeltaPathDist = deltaPathDist(edgeNode, *e.source);
        if (currDeltaPathDist < minDeltaPathDist || bestCandidate == nullptr) {
            bestCandidate = &e;
            bestCandidateNeighbor = e.source;
            minDeltaPathDist = currDeltaPathDist;
        }
    }
    for (Edge &e : edgeNode.edgesOut) {
        if (!isDistributary(e.target->type))
            continue;
        float currDeltaPathDist = deltaPathDist(edgeNode, *e.target);
        if (currDeltaPathDist < minDeltaPathDist || bestCandidate == nullptr) {
            bestCandidate = &e;
            bestCandidateNeighbor = e.target;
            minDeltaPathDist = currDeltaPathDist;
        }
    }
    if (bestCandidate != nullptr) {
        edgeNode.crossChannelA = bestCandidate;
        if (bestCandidateNeighbor->crossChannelA == nullptr) {
            assignCrossChannelEdgesFromNode(*bestCandidateNeighbor);
        }
        if (bestCandidateNeighbor->crossChannelA != nullptr) {
            edgeNode.crossChannelA = bestCandidateNeighbor->crossChannelA;
            if (bestCandidateNeighbor->crossChannelB != nullptr) {
                edgeNode.crossChannelB = bestCandidateNeighbor->crossChannelA;
            }
        }
    }
}

void assignCrossChannelEdgesFromNode(MapNode &node) {
    if (node.type == HabitatType::DistributaryEdge) {
        assignCrossChannelEdgesFromChannelEdge(node);
    } else {
        assignCrossChannelEdgesFromChannelCenter(node);
    }
}

void assignCrossChannelEdges(std::vector<MapNode *> &map) {
    for (MapNode *node : map) {
        if (isDistributary(node->type) && countAdjacentDistributaryNodes(*node) > 2 && node->crossChannelA == nullptr) {
            assignCrossChannelEdgesFromNode(*node);
        }
    }
}
*/

// Clean up edges that point to deleted or nonexistent nodes
void fixBrokenEdges(std::vector<MapNode *> &map) {
    // Make a hash set to be able to easily check if a given node
    // is part of the map node list
    std::unordered_set<MapNode *> allNodes;
    for (MapNode *node : map) {
        allNodes.insert(node);
    }
    // For each map node, go through its edges and remove any that don't point to a node
    // that's actually in the map
    for (MapNode *node : map) {
        bool modified = true;
        while (modified) {
            modified = false;
            for (auto it = node->edgesIn.begin(); it != node->edgesIn.end(); ++it) {
                if (!allNodes.count(it->source)) {
                    node->edgesIn.erase(it);
                    modified = true;
                    break;
                }
            }
        }
        modified = true;
        while (modified) {
            modified = false;
            for (auto it = node->edgesOut.begin(); it != node->edgesOut.end(); ++it) {
                if (!allNodes.count(it->target)) {
                    node->edgesOut.erase(it);
                    modified = true;
                    break;
                }
            }
        }
    }
}

// Mark "distributaries" that aren't connected to the actual distributary network
// as blind channels
// TODO: GROT
void fixDisjointDistributaries(std::vector<MapNode *> &map, std::vector<MapNode *> &recPoints, std::unordered_set<MapNode *> &protectedNodes) {
    std::unordered_set<MapNode *> connected;
    std::vector<MapNode *> fringe;
    // Do a graph traversal to find which nodes are connected to the distributary network
    // Start from the recruit point
    for (MapNode *node : recPoints) {
        fringe.push_back(node);
    }
    while (fringe.size() != 0) {
        MapNode *curr = fringe.back();
        fringe.pop_back();
        if (!connected.count(curr)) {
            connected.insert(curr);
            for (Edge &e : curr->edgesIn) {
                fringe.push_back(e.source);
            }
            for (Edge &e : curr->edgesOut) {
                fringe.push_back(e.target);
            }
        }
    }
    unsigned corrected = 0;
    unsigned edgeless = 0;
    unsigned orphaned_protected = 0;
    unsigned disconnected_count = 0;
    for (MapNode *node : map) {
        bool disconnected = !connected.count(node);
        if (disconnected) {
            ++disconnected_count;
        }
        if (disconnected && node->edgesIn.empty() && node->edgesOut.empty()) {
            ++edgeless;
            if (protectedNodes.count(node)) {
                ++orphaned_protected;
            }
        }
        if (isDistributary(node->type) && disconnected) {
            node->type = HabitatType::BlindChannel;
            ++corrected;
        }
    }
    std::cout << "Made  " << corrected << " disconnected 'distributary' nodes into blind channels" << std::endl;
    std::cout << "Found " << disconnected_count << " disconnected nodes" << std::endl;
    std::cout << "Found " << edgeless << " orphaned nodes" << std::endl;
    std::cout << "Found " << orphaned_protected << " orphaned protected nodes" << std::endl;
}

void cleanupRemovedNodeIdMappings(std::unordered_map<unsigned int, unsigned int> &csvToInternalId, const std::vector<MapNode *> &dest) {
    for (auto csv_it =  csvToInternalId.begin(); csv_it != csvToInternalId.end(); ) {
        int internalId = static_cast<int>(csv_it->second);
        auto found_internal = std::find_if(dest.begin(), dest.end(),[internalId](const MapNode *node) -> bool { return (node->id == internalId); });
        bool unused_mapping = (found_internal == dest.end());
        if (unused_mapping) {
            csv_it = csvToInternalId.erase(csv_it);
        } else {
            ++csv_it;
        }
    }
}

void identifyProtectedNodes(const std::vector<MapNode *> &monitoringPoints, const std::unordered_map<MapNode *, SamplingSite *> &samplingSitesByNode, const std::vector<MapNode *> &recPoints, std::unordered_set<MapNode *> &protectedNodes) {
    for (MapNode *node : monitoringPoints) {
        protectedNodes.insert(node);
    }
    for (auto nodePair : samplingSitesByNode) {
        protectedNodes.insert(nodePair.first);
    }
    for (MapNode *node : recPoints) {
        protectedNodes.insert(node);
    }
}

std::unordered_set<MapNode *> identifyDisconnectedNodes(const std::vector<MapNode *> & map, const std::vector<MapNode *> &recruitPoints) {
    std::unordered_set<MapNode *> connectedNodes;
    std::vector<MapNode *> walker;
    // Do a graph traversal to find which nodes are connected to the distributary network
    // Start from the recruit point
    for (MapNode *node : recruitPoints) {
        walker.push_back(node);
    }
    while (!walker.empty()) {
        MapNode *node = walker.back();
        walker.pop_back();
        if (!connectedNodes.count(node)) {
            connectedNodes.insert(node);
            for (Edge &e : node->edgesIn) {
                walker.push_back(e.source);
            }
            for (Edge &e : node->edgesOut) {
                walker.push_back(e.target);
            }
        }
    }

    std::unordered_set<MapNode *> disconnectedNodes;
    for (MapNode *node : map) {
        if (connectedNodes.count(node) == 0) {
            disconnectedNodes.insert(node);
        }
    }
    return disconnectedNodes;
}


void removeDisconnectedNodes(const std::unordered_set<MapNode *> &disconnectedNodes, std::vector<MapNode *> &map,
                             std::vector<MapNode *> &recruitPoints, std::vector<MapNode *> &monitoringPoints,
                             std::vector<SamplingSite *> &samplingSites,
                             std::unordered_map<MapNode *, SamplingSite *> &samplingSitesByNode) {
    for (MapNode *disconnectedNode : disconnectedNodes) {
        auto recruitIter = std::find(recruitPoints.begin(), recruitPoints.end(), disconnectedNode);
        if (recruitIter != recruitPoints.end()) {
            recruitPoints.erase(recruitIter);
        }

        auto monitoringPointsIter = std::find(monitoringPoints.begin(), monitoringPoints.end(), disconnectedNode);
        if (monitoringPointsIter != monitoringPoints.end()) {
            monitoringPoints.erase(monitoringPointsIter);
        }

        SamplingSite* samplingSite = nullptr;
        auto samplingNodeMapIter = samplingSitesByNode.find(disconnectedNode);
        if (samplingNodeMapIter != samplingSitesByNode.end()) {
            samplingSite = samplingNodeMapIter->second;
            samplingSitesByNode.erase(samplingNodeMapIter);
        }

        if (samplingSite != nullptr) {
            auto pointsIter = std::find(samplingSite->points.begin(), samplingSite->points.end(), disconnectedNode);
            if (pointsIter != samplingSite->points.end()) {
                samplingSite->points.erase(pointsIter);
            }

            if (samplingSite->points.empty()) {
                auto samplingSitesIter = std::find(samplingSites.begin(), samplingSites.end(), samplingSite);
                if (samplingSitesIter != samplingSites.end()) {
                    samplingSites.erase(samplingSitesIter);
                    delete samplingSite;
                }
            }
        }

        auto mapIter = std::find(map.begin(), map.end(), disconnectedNode);
        if (mapIter != map.end()) {
            map.erase(mapIter);
            delete disconnectedNode;
        }
    }
}

// Load a map from vertex, edge, and geometry files
// (additionally runs cleanup on the resulting map graph)
void loadMap(
    std::vector<MapNode *> &dest,
    std::string locationFilePath,
    std::string edgeFilePath,
    std::string geometryFilePath,
    std::unordered_map<unsigned int, unsigned int> &csvToInternalID,
    std::vector<DistribHydroNode> &hydroNodes,
    std::vector<unsigned> &recPointIds,
    std::vector<MapNode *> &recPoints,
    std::vector<MapNode *> &monitoringPoints,
    std::vector<SamplingSite *> &samplingSites,
    float blindChannelSimplificationRadius
) {
    std::ifstream locationFile;
    locationFile.open(locationFilePath);
    std::ifstream edgeFile;
    edgeFile.open(edgeFilePath);
    std::ifstream geometryFile;
    geometryFile.open(geometryFilePath);
    std::string line;
    bool first = true;
    std::unordered_map<std::string, SamplingSite *> samplingSitesByName;
    std::unordered_map<MapNode *, SamplingSite *> samplingSitesByNode;
    csvToInternalID.clear();
    dest.clear();
    // Load node data rom the vertex file
    while (std::getline(locationFile, line)) {
        if (first) {
            // Skip the first line, it's a field name header
            first = false;
            continue;
        }
        std::vector<std::string> chunks = split(line, ',');
        // create a MapNodeData struct to temporarily hold the relevant data until we know
        // an ID to make an actual MapNode with
        // Check the MapNodeData definition for field names
        unsigned int csvId = std::stoi(chunks[0]);
        unsigned int internalId = dest.size();
        if (csvToInternalID.count(csvId)) {
            std::cerr << "Multiple nodes with ID " << csvId << "!" << std::endl;
            continue;
        }
        csvToInternalID[csvId] = internalId;
        float area = std::stof(chunks[5]);
        float sourceDistance = std::stof(chunks[7]);
        float elev = std::stof(chunks[10]);
        HabitatType habType = std::stoi(chunks[11]) == 1 ? HabitatType::DistributaryEdge : habTypeByName.at(chunks[6]);

        dest.push_back(new MapNode(
            habType, area, elev, sourceDistance
        ));
        MapNode *node = dest.back();
        node->id = internalId;

        if (std::stoi(chunks[12]) == 1) {
            monitoringPoints.push_back(node);
        }
        std::string siteName = chunks[16];
        if (siteName.length() > 0 && siteName[siteName.length()-1] == '\r') {
            siteName = siteName.substr(0, siteName.length() - 1);
        }

        if (siteName.length() > 0) {
            SamplingSite *site = nullptr;
            if (samplingSitesByName.count(siteName)) {
                site = samplingSitesByName[siteName];
            } else {
                samplingSites.push_back(new SamplingSite(siteName, samplingSites.size()));
                site = samplingSites.back();
                samplingSitesByName[siteName] = site;
            }
            site->points.push_back(node);
            samplingSitesByNode[node] = site;
        }
    }
    unsigned int maxRealID = dest.size() - 1;
    // Load edges from the edge file
    first = true;
    while (std::getline(edgeFile, line)) {
        if (first) {
            // Skip the first line, it's a field name header
            first = false;
            continue;
        }
        std::vector<std::string> chunks = split(line, ',');
        if (chunks[14].length() == 0) {
            std::cerr << "Edge " << chunks[1] << " missing source node!" << std::endl;
            continue;
        }
        if (chunks[15].length() == 0) {
            std::cerr << "Edge " << chunks[1] << " missing target node!" << std::endl;
            continue;
        }
        unsigned idSource = std::stoi(chunks[14]);
        unsigned idTarget = std::stoi(chunks[15]);
        if (!(csvToInternalID.count(idSource) && csvToInternalID.count(idTarget))) {
            std::cerr << "Edge " << chunks[1] << " has nonexistent source/target!" << std::endl;
            continue;
        }
        float length = std::stof(chunks[18]);
        Edge e(dest[csvToInternalID[idSource]], dest[csvToInternalID[idTarget]], length);
        // Check to make sure this edge isn't redundant before adding it
        bool redundant = false;
        for (Edge &e2 : e.source->edgesIn) {
            if (e2.source == e.target) {
                redundant = true;
                break;
            }
        }
        if (!redundant) {
            dest[csvToInternalID[idSource]]->edgesOut.push_back(e);
            dest[csvToInternalID[idTarget]]->edgesIn.push_back(e);
        }
    }
    // Load node locations from the geometry file
    first = true;
    while (std::getline(geometryFile, line)) {
        if (first) {
            first = false;
            continue;
        }
        std::vector<std::string> chunks = split(line, ',');
        unsigned id = std::stoi(chunks[2]);
        if (!csvToInternalID.count(id)) {
            std::cerr << "Geometry file references nonexistent node " << id << std::endl;
            continue;
        }
        dest[csvToInternalID[id]]->x = std::stof(chunks[0]);
        dest[csvToInternalID[id]]->y = std::stof(chunks[1]);
    }
    for (unsigned id : recPointIds) {
        if (!csvToInternalID.count(id)) {
            std::cerr << "Recruitment node " << id << " doesn't exist" << std::endl;
        }
        recPoints.push_back(dest[csvToInternalID[id]]);
    }
    // Clean up clean up everybody do your share
    //condenseMissingNodes(dest);
    std::unordered_set<MapNode *> disconnectedNodes = identifyDisconnectedNodes(dest, recPoints);
    removeDisconnectedNodes(disconnectedNodes, dest, recPoints, monitoringPoints, samplingSites, samplingSitesByNode);

    std::unordered_set<MapNode *> protectedNodes;
    identifyProtectedNodes(monitoringPoints, samplingSitesByNode, recPoints, protectedNodes);
    simplifyBlindChannels(dest, blindChannelSimplificationRadius, protectedNodes);
    //fixBrokenEdges(dest);
    expandNearshoreLinks(dest, maxRealID);
    //assignCrossChannelEdges(dest); // OBSOLETE
    fixDisjointDistributaries(dest, recPoints, protectedNodes);
    assignNearestHydroNodes(dest, hydroNodes);
    fixElevations(dest, hydroNodes);
    cleanupRemovedNodeIdMappings(csvToInternalID, dest);
}