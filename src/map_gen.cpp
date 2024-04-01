#include "map_gen.h"
#include "map.h"
#include "util.h"
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <tuple>
#include <iostream>

// Adds an edge from the node "source" to the node "target"
bool addEdge(MapNode *source, MapNode *target) {
    // Check to make sure we're not adding a redundant edge
    for (auto it = source->edgesOut.begin(); it != source->edgesOut.end(); ++it) {
        if (it->target == target) {
            return false;
        }
    }
    // Create the edge and add it to the nodes' edge lists
    Edge newEdge(source, target, getDistance(source, target));
    source->edgesOut.push_back(newEdge);
    target->edgesIn.push_back(newEdge);
    return true;
}

// Creates an edge between two nodes, additionally deciding
// which direction the edge goes using node habitat types and coin flips
bool connect(MapNode *a, MapNode *b, bool &reverse) {
    reverse = false;
    if (a->type == HabitatType::Distributary) {
        if (b->type == HabitatType::BlindChannel || b->type == HabitatType::Nearshore) {
            // Blind channels are always downstream of distributary nodes
            return addEdge(a, b);
        } else if (b->type == HabitatType::Distributary) {
            // Distributary<->distributary edges go downstream north -> south
            // unless distributary nodes are at an equal latitude (y) in which case
            // they are random
            if (a->y > b->y || (a->y == b->y && unit_rand() < 0.5f)) {
                return addEdge(a, b);
            } else {
                reverse = true;
                return addEdge(b, a);
            }
        }
    } else if (a->type == HabitatType::BlindChannel) {
        if (b->type == HabitatType::Distributary) {
            // Blind channels are always downstream of distributary nodes
            return addEdge(b, a);
        } else if (b->type == HabitatType::BlindChannel) {
            // Blind channel<->blind channel edges are random
            if (unit_rand() < 0.5) {
                return addEdge(a, b);
            } else {
                reverse = true;
                return addEdge(b, a);
            }
        } else if (b->type == HabitatType::Nearshore) {
            // Nearshore nodes are downstream of everything else
            return addEdge(a, b);
        }
    } else if (a->type == HabitatType::Nearshore) {
        if (b->type == HabitatType::Nearshore) {
            // Nearshore nodes don't connect to each other
            return false;
        } else {
            reverse = true;
            // Nearshore nodes are downstream of everything else
            return addEdge(b, a);
        }
    }
    return false;
}

// Removes any edge connecting two nodes (in the specified direction)
void removeEdgeBetween(MapNode *source, MapNode *target) {
    // Remove from source's outgoing list
    for (auto it = source->edgesOut.begin(); it != source->edgesOut.end(); ++it) {
        if (it->target == target) {
            source->edgesOut.erase(it);
            break;
        }
    }
    // Remove from target's incoming list
    for (auto it = target->edgesIn.begin(); it != target->edgesIn.end(); ++it) {
        if (it->source == source) {
            target->edgesIn.erase(it);
            break;
        }
    }
}

// Alias for removeEdgeBetween that takes an edge instead of two nodes
void removeEdgeBetween(Edge &e) {
    removeEdgeBetween(e.source, e.target);
}

// Bidirectional removeEdgeBetween
void disconnect(MapNode *a, MapNode *b) {
    removeEdgeBetween(a, b);
    removeEdgeBetween(b, a);
}

// Checks to see whether any of the nodes in the list "sourceNodes"
// are connected through the map network to any of the nodes in the set "targetNodes"
// WITHOUT traversing any edge between nodes "querySource" and "queryTarget"
bool connectedWithout(
    std::vector<MapNode *> &sourceNodes,
    std::unordered_set<MapNode *> &targetNodes,
    MapNode *querySource, MapNode *queryTarget
) {
    // A set to keep track of nodes we've already explored
    std::unordered_set<MapNode *> visited;
    // List of nodes to explore next
    std::vector<MapNode *> fringe(sourceNodes);
    // Keep going until no nodes are yet to be explored
    while (fringe.size() > 0) {
        // Pick a node from the fringe and remove it from the fringe
        MapNode *node = fringe.back();
        fringe.pop_back();
        // Check if we've reached any of our targets
        if (targetNodes.count(node)) {
            return true;
        } else {
            // Check if we've already been here, if so, move on
            if (visited.count(node)) {
                continue;
            }
            // Otherwise, check to see where we can go from here
            // (we can only traverse distributaries, and we can't cross between the query nodes)
            // Put all newly reachable nodes on the fringe
            visited.insert(node);
            for (auto it = node->edgesOut.begin(); it != node->edgesOut.end(); ++it) {
                if (it->target->type == HabitatType::Distributary
                    && !(it->source == querySource && it->target == queryTarget)) {
                    fringe.push_back(it->target);
                }
            }
            for (auto it = node->edgesIn.begin(); it != node->edgesIn.end(); ++it) {
                if (it->source->type == HabitatType::Distributary
                    && !(it->source == querySource && it->target == queryTarget)) {
                    fringe.push_back(it->source);
                }
            }
        }
    }
    // If we've done an exhaustive search starting from the source nodes without finding any targets,
    // we can safely say the sourceNodes and targetNodes are not connected in the absence of an edge
    // between querySource and queryTarget
    return false;
}

// Calculates shortest path distances to all nodes in the network starting from nodes in the "sourceNodes" list
void findDistances(std::vector<MapNode *> &sourceNodes, std::unordered_map<MapNode *, float> &distances) {
    // List of pairs of (node, distance) that are on the edge of our graph traversal
    std::vector<std::tuple<MapNode *, float>> fringe;
    // Create our initial fringe entries from the nodes in sourceNodes (with distance 0)
    for (MapNode *node : sourceNodes) {
        fringe.emplace_back(node, 0.0f);
    }
    // Keep going until we've hit all reachable nodes
    // We'll use the "distances" map to keep track of which nodes we've already traversed
    while (fringe.size() > 0) {
        // Pick a node & distance off the fringe
        std::tuple<MapNode *, float> pair = fringe.back();
        fringe.pop_back();
        MapNode *node = std::get<0>(pair);
        float dist = std::get<1>(pair);
        // If we have already been to this node and it took less than or equal to the same distance
        // to get here that time, ignore this fringe entry and move on
        if (distances.count(node) && distances[node] <= dist) {
            continue;
        }
        // Otherwise, we know this is the new shortest way to get here so record that in the map
        distances[node] = dist;
        // And add any newly reachable nodes to the fringe with the current distance plus
        // the edge length to get there from this node
        for (Edge &e : node->edgesOut) {
            if (!distances.count(e.target) || distances[e.target] > dist + e.length) {
                fringe.emplace_back(e.target, dist+e.length);
            }
        }
        for (Edge &e : node->edgesIn) {
            if (!distances.count(e.source) || distances[e.source] > dist + e.length) {
                fringe.emplace_back(e.source, dist+e.length);
            }
        }
    }
}

// Utility definitions to provide hashing & equality functions for using a
// map with pairs of ints as keys
typedef std::tuple<int, int> map_coord_t;

struct coord_hash : public std::unary_function<map_coord_t, std::size_t>
{
    std::size_t operator()(const map_coord_t& k) const
    {
        return std::get<0>(k) ^ std::get<1>(k);
    }
};

struct coord_equal : public std::binary_function<map_coord_t, map_coord_t, bool>
{
    bool operator()(const map_coord_t& v0, const map_coord_t& v1) const
    {
        return (
            std::get<0>(v0) == std::get<0>(v1) &&
            std::get<1>(v0) == std::get<1>(v1)
        );
    }
};

// Generate a map, placing the resulting nodes in the "dest" list and the resulting recruitment points
// in the "recPoints" list
// m is the width of the map on each axis
// n is the number of nodes along each axis
// a is the distance in meters between each node
// p_dist is the probability of deleting a given distributary link
// p_blind is the probability of deleting a given blind channel link
void generateMap(
    std::vector<MapNode *> &dest,
    std::vector<MapNode *> &recPoints,
    int m,
    int n,
    float a,
    float p_dist,
    float p_blind
) {
    std::unordered_map<map_coord_t, MapNode *, coord_hash, coord_equal> nodes;
    std::vector<MapNode *> nodeList;
    std::vector<MapNode *> topNodes;
    std::unordered_set<MapNode *> nearshoreNodes;
    std::vector<std::tuple<MapNode *, MapNode *>> removableEdges;
    int d = (n / m) + 1;
    // Check to make sure the distributary subdivisions will be even
    if ((n - 1) % d != 0) {
        std::cerr << "Grid misalignment" << std::endl;
    }
    // n + 1 rows
    for (int i = 0; i < n + 1; ++i) {
        // n cols
        for (int j = 0; j < n; ++j) {
            // Habitat type is distributary when we're on an even multiple of d rows & cols
            HabitatType habType = (i % d == 0 && j % d == 0) ? HabitatType::Distributary : HabitatType::BlindChannel;
            if (i == n) {
                habType = HabitatType::Nearshore;
            }
            // Calculate the new node's area
            float area = a*a;
            if (habType == HabitatType::Distributary) {
                area *= 3.0f;
                if (i == 0) {
                    area *= 3.0f;
                }
            }
            // Make the new node
            MapNode *node = new MapNode(habType, area, 0.0f, 0.0f);
            node->x = ((float) j) * a;
            node->y = ((float) (n - i)) * a;
            nodes[map_coord_t(i, j)] = node;
            nodeList.push_back(node);
            // NOTE: must initialize pathDist after construction
            // NOTE: elev uninitialized
            if (i == 0) {
                topNodes.push_back(node);
            }
            if (habType == HabitatType::Nearshore) {
                nearshoreNodes.insert(node);
            }
            // Connect to adjacent nodes
            // Neighbor one row above
            if (nodes.count(map_coord_t(i - 1, j))) {
                bool reverse;
                MapNode *buddy = nodes[map_coord_t(i - 1, j)];
                if (connect(buddy, node, reverse) && i != n) {
                    removableEdges.emplace_back(reverse ? node : buddy, reverse ? buddy : node);
                }
            }
            // Neighbor one col to the left
            if (nodes.count(map_coord_t(i, j - 1)) && habType != HabitatType::Nearshore) {
                bool reverse;
                MapNode *buddy = nodes[map_coord_t(i, j - 1)];
                if (connect(buddy, node, reverse) && i != n - 1) {
                    removableEdges.emplace_back(reverse ? node : buddy, reverse ? buddy : node);
                }
            }
            // Connect to distributary nodes if this is a distributary node
            if (habType == HabitatType::Distributary) {
                // "Neighbor" d rows above
                if (nodes.count(map_coord_t(i - d, j))) {
                    bool reverse;
                    MapNode *buddy = nodes[map_coord_t(i - d, j)];
                    if (connect(buddy, node, reverse)) {
                        removableEdges.emplace_back(reverse ? node : buddy, reverse ? buddy : node);
                    }
                }
                // "Neighbor" d cols to the left
                if (nodes.count(map_coord_t(i, j - d))) {
                    bool reverse;
                    MapNode *buddy = nodes[map_coord_t(i, j - d)];
                    if (connect(buddy, node, reverse)) {
                        removableEdges.emplace_back(reverse ? node : buddy, reverse ? buddy : node);
                    }
                }
            }
        }
    }
    // Run through all the optional edges we've added 
    // and prune some randomly according to p_blind and p_dist
    for (std::tuple<MapNode *, MapNode *> &pair : removableEdges) {
        MapNode *source = std::get<0>(pair);
        MapNode *target = std::get<1>(pair);
        if ((
                source->type == HabitatType::Distributary && target->type == HabitatType::Distributary
                && unit_rand() < p_dist && connectedWithout(topNodes, nearshoreNodes, source, target)
            ) || (
                (source->type == HabitatType::BlindChannel || target->type == HabitatType::BlindChannel)
                && unit_rand() < p_blind
            )
        ) {
            removeEdgeBetween(source, target);
        }
    }
    // Calculate path distances (from source nodes at the top)
    std::unordered_map<MapNode *, float> distances;
    findDistances(topNodes, distances);
    // Get rid of nodes that aren't in the network anymore (aren't connected to top nodes transitively)
    std::vector<MapNode *> nodesToDelete;
    for (MapNode *node : nodeList) {
        if (!distances.count(node)) {
            for (Edge &e : node->edgesOut) {
                for (auto it = e.target->edgesIn.begin(); it != e.target->edgesIn.end(); ++it) {
                    if (it->source == node) {
                        e.target->edgesIn.erase(it);
                        break;
                    }
                }
            }
            for (Edge &e : node->edgesIn) {
                for (auto it = e.source->edgesOut.begin(); it != e.source->edgesOut.end(); ++it) {
                    if (it->target == node) {
                        e.source->edgesOut.erase(it);
                        break;
                    }
                }
            }
            nodesToDelete.push_back(node);
        } else {
            dest.push_back(node);
            node->pathDist = distances[node];
            // TODO check polarity of cpp pathDist field (increase as approach source? or nearshore?)
        }
    }
    // Recruits can enter at any top node
    // TODO: Make sure reachability check includes pathing to nearshore
    std::unordered_map<MapNode *, float> distancesToNearshore;
    std::vector<MapNode *> nearshoreList;
    for (MapNode *node : nearshoreNodes) {
        nearshoreList.push_back(node);
    }
    findDistances(nearshoreList, distancesToNearshore);
    for (MapNode *node : topNodes) {
        if (distancesToNearshore.count(node) && node->type == HabitatType::Distributary) {
            recPoints.push_back(node);
        }
    }
    for (MapNode *node : nodesToDelete) {
        delete node;
    }
    //assignIDs(dest);
    return;
}