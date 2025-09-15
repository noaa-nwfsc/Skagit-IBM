#include "fish.h"
#include "fish_movement.h"

#include <deque>
#include <cmath>
#include <iostream>
#include <unordered_set>
#include <utility>
#include "util.h"

const float CA = 0.303;
const float CB = -0.275;
const float CQ = 5.0;
const float CTO = 15.0;
const float CTM = 25.0; // was 18.0
//const float CTL = 24.0;
//const float CK1 = 0.36;
//const float CK4 = 0.01;
// Respiration (Equation 1)
const float RA = 0.00264;
const float RB = -0.217;
const float RQ = 0.06818;
const float RTO = 0.0234;
//const float RTM = 0.0;
//const float RTL = 25.0;
//const float RK1 = 1.0;
//const float RK4 = 0.13;
//const float ACT = 9.7;
//const float BACT = 0.0405;
const float SDA = 0.172;
// Egestion (uses Equation set 2, for now)
const float FA = 0.212;
const float FB = -0.222;
const float FG = 0.631;
// Excretion
const float UA = 0.0314;
const float UB = 0.58;
const float UG = -0.299;

// Pmax params
const float consA = 1; // maybe play with this
const float consV = 5; // maybe play with this - assumes there is an excess of food

// Consumption (g*g^-1*d^-1)
const float CONS_Z = log(CQ) * (CTM - CTO);
const float CONS_Y = log(CQ) * (CTM - CTO + 2.0);
const float CONS_X = (pow(CONS_Z, 2.0) * pow(1.0 + sqrt(1.0 + 40/CONS_Y), 2.0))/400.0;

const float AVG_LOCAL_ABUNDANCE = 7.5839;

// Convert a fork length value (in mm) to a mass value (in g)
// Note: the resulting value is slightly stochastic
inline float massFromForkLength(float forkLength) {
    return fmax(0.15f, 4.090e-06f*pow(forkLength, 3.218f) + unit_normal_rand()*0.245307f);
}

// Convert a mass value (in g) to a fork length value (in mm)
// Note: the resulting value is slightly stochastic
inline float forkLengthFromMass(float mass) {
    return fmax(20.0f, 47.828851f*pow(mass, 0.292476f) + unit_normal_rand()*2.07895f);
}

// Haefner et al. 2002
// This is a sustained swim speed
const float SWIM_SPEED_BODY_LENGTHS_PER_SEC = 2.0f;

constexpr  float HOURS_PER_TIMESTEP = 1.0f;
constexpr  float SECONDS_PER_TIMESTEP = HOURS_PER_TIMESTEP * 60.0f*60.0f;

// Calculate a sustained-movement swim range (in m) from a fork length (in mm)
inline float swimSpeedFromForkLength(float forkLength) {
    // body lengths per sec (s^-1) * (s/m) * (m/h) * body length (mm) * (m/mm);
    return SWIM_SPEED_BODY_LENGTHS_PER_SEC * forkLength * 0.001f;
}

// Fish constructor
// Initializes a fish from a starting location, timestep, and fork length
// (mass is calculated from fork length)
Fish::Fish(
        unsigned long id,
        long spawnTime,
        float forkLength,
        MapNode *location
    ) : id(id),
        spawnTime(spawnTime),
        entryForkLength(forkLength),
        forkLength(forkLength),
        mass(massFromForkLength(forkLength)),
        location(location),
        travel(0),
        status(FishStatus::Alive),
        exitStatus(FishStatus::Alive),
        numExitHabitatHours(0),
        lastGrowth(0),
        lastPmax(0),
        lastMortality(0),
        lastTemp(0),
        lastDepth(0),
        lastFlowSpeed_old(0),
        lastFlowVelocity(0, 0),
        taggedTime(-1L),
        locationHistory(nullptr),
        pmaxHistory(nullptr),
        growthHistory(nullptr),
        mortalityHistory(nullptr),
        tempHistory(nullptr),
        depthHistory(nullptr),
        flowSpeedHistory_old(nullptr),
        flowVelocityHistory(nullptr),
        massHistory(nullptr),
        forkLengthHistory(nullptr)
    {this->entryMass = this->mass;}


/*
    Movement notes:
        - Correlated random walk
        - Until local maximum fitness/max range reached
*/
/* OLD
void Fish::getReachableNodes(Model &model, std::unordered_map<MapNode *, float> &out)
{
    //TODO: use (correlated) random walk instead of dijkstra for exploration

    // This set contains all nodes that have already been traversed and shouldn't
    // be re-evaluated
    std::unordered_set<MapNode *> visited;
    // This is a list of candidate nodes (and their swim costs) accessible from explored nodes
    std::deque<std::pair<MapNode *, float>> fringe;
    // Add the starting node (this fish's current location)
    fringe.emplace_back(this->location, 0.0f);
    // Calculate swim range & speed once
    float swimRange = rangeFromForkLength(this->forkLength);
    float swimSpeed = swimSpeedFromRange(swimRange);
    // Loop until there are no new candidate nodes
    while (fringe.size() > 0) {
        // Pick a node off the fringe to evaluate
        std::pair<MapNode *, float> popped = fringe.front();
        MapNode *point = popped.first;
        float cost = popped.second;
        // (remove the selected node)
        fringe.pop_front();
        // Check if the node has already been evaluated
        if (visited.count(point) == 0) {
            // Mark it as evaluated
            visited.insert(point);
            // This variable keeps track of whether any adjacent nodes are
            // within range (if not, this node should be marked as accessible even if it's outside the swimmable range,
            // to make sure there's at least 1 accessible node)
            bool hasChildren = false;
            // Check all channels flowing into this node
            for (Edge &edge : point->edgesIn) {
                if (!visited.count(edge.source) && model.hydroModel.getDepth(*edge.source) >= DEPTH_CUTOFF) {
                    // Effective movement speed along channel (swim speed adjusted by flow speed)
                    float transitSpeed = swimSpeed - model.hydroModel.getFlowSpeedAlong(edge);
                    // Check to make sure the fish can even make progress
                    if (transitSpeed > 0.0f) {
                        // Calculate effective distance swum
                        float edgeCost = (edge.length/transitSpeed)*swimSpeed;
                        if (isDistributary(edge.source->type) && point == this->location) {
                            // Artificially discount the cost to make distributary nodes easier to access
                            // (since they are widely spaced)
                            edgeCost = std::min(edgeCost, swimRange - cost);
                        }
                        // Check if connected node is reachable
                        if (cost + edgeCost <= swimRange) {
                            hasChildren = true;
                            // Add it to the fringe with the new total cost
                            fringe.emplace_back(edge.source, cost + edgeCost);
                        }
                    }
                }
            }
            // Check all channels flowing out of this node
            // Same as above (except flow directions are reversed)
            for (Edge &edge : point->edgesOut) {
                if (!visited.count(edge.target) && model.hydroModel.getDepth(*edge.target) >= DEPTH_CUTOFF) {
                    float transitSpeed = swimSpeed + model.hydroModel.getFlowSpeedAlong(edge);
                    if (transitSpeed > 0.0f) {
                        float edgeCost = (edge.length/transitSpeed)*swimSpeed;
                        if (isDistributary(edge.target->type) && point == this->location) {
                            edgeCost = std::min(edgeCost, swimRange - cost);
                        }
                        if (cost + edgeCost <= swimRange) {
                            hasChildren = true;
                            fringe.emplace_back(edge.target, cost + edgeCost);
                        }
                    }
                }
            }
            // Calculate the swim cost of remaining at this node for the rest of the timestep
            float stayCost = cost + model.hydroModel.getFlowSpeedAt(*point)*(60.0f*60.0f - cost / swimSpeed);
            if (stayCost <= swimRange || !hasChildren) {
                // If this node is reachable (either naturally or because of the no-children safeguard)
                // add it to the "out" map
                out[point] = std::min(swimRange, stayCost);
            }
        }
    }
}
*/

/*
 * Fill the "out" map with a mapping from nodes to effective distance swum to reach that node.
 * If a node is present in the resulting map, it means that node is reachable in the current timestep.
 * The "effective distance swum" value associated with that node indicates how far this fish would
 *   have to swim to reach it (including any extra distance added by swimming against the flow,
 *   or any distance removed by swimming with the flow)
 */
void Fish::getReachableNodes(Model &model, std::unordered_map<MapNode *, float> &out)
{
    // This is a list of candidate nodes (and their swim costs) accessible from explored nodes
    std::deque<std::tuple<MapNode *, float>> fringe;
    // Add the starting node (this fish's current location)
    fringe.emplace_back(this->location,  0.0f);
    // Calculate swim range & speed once
    float swimSpeed = swimSpeedFromForkLength(this->forkLength);
    float swimRange = swimSpeed*SECONDS_PER_TIMESTEP;
    // Loop until there are no new candidate nodes
    while (fringe.size() > 0) {
        // Pick a node off the fringe to evaluate
        std::tuple<MapNode *, float> popped = fringe.front();
        MapNode *point = std::get<0>(popped);
        float cost = std::get<1>(popped);
        out[point] = cost;
        // (remove the selected node)
        fringe.pop_front();
        // Check all channels flowing into this node
        for (Edge &edge : point->edgesIn) {
            if (!out.count(edge.source) && model.hydroModel.getDepth(*edge.source) >= MOVEMENT_DEPTH_CUTOFF) {
                // Effective movement speed along channel (swim speed adjusted by flow speed)
                float transitSpeed = swimSpeed - model.hydroModel.getFlowSpeedAlong(edge);
                // Check to make sure the fish can even make progress
                if (transitSpeed > 0.0f) {
                    // Calculate effective distance swum
                    float edgeCost = (edge.length/transitSpeed)*swimSpeed;
                    if (isDistributary(edge.source->type) && point == this->location){ //  || (this->forkLength >= 75)){
                        // Artificially discount the cost to make distributary nodes easier to access
                        // (since they are widely spaced)
                        edgeCost = std::min(edgeCost, swimRange - cost);
                    }
                    // Check if connected node is reachable
                    if (cost + edgeCost <= swimRange) {
                        // Add it to the fringe with the new total cost
                        fringe.emplace_back(edge.source, cost + edgeCost);
                    }
                }
            }
        }
        // Check all channels flowing out of this node
        // Same as above (except flow directions are reversed)
        for (Edge &edge : point->edgesOut) {
            if (!out.count(edge.target) && model.hydroModel.getDepth(*edge.target) >= MOVEMENT_DEPTH_CUTOFF) {
                float transitSpeed = swimSpeed + model.hydroModel.getFlowSpeedAlong(edge);
                if (transitSpeed > 0.0f) {
                    float edgeCost = (edge.length/transitSpeed)*swimSpeed;
                    if (isDistributary(point->type) && point == this->location){ // || (this->forkLength >= 75)){
                        edgeCost = std::min(edgeCost, swimRange - cost);
                    }
                    if (cost + edgeCost <= swimRange) {
                        fringe.emplace_back(edge.target, cost + edgeCost);
                    }
                }
            }
        }
    }
}

/*
 * Fill the "out" map with a mapping from nodes to probabilities that this fish will arrive at that node.
 */
void Fish::getDestinationProbs(Model &model, std::unordered_map<MapNode *, float> &out)
{
    // This is a list of candidate nodes (and their swim costs) accessible from explored nodes
    std::deque<std::tuple<MapNode *, float, float, float>> fringe;
    // Add the starting node (this fish's current location)
    fringe.emplace_back(this->location,  0.0f, this->getFitness(model, *this->location, 0.0f), 1.0f);
    // Calculate swim range & speed once
    float swimSpeed = swimSpeedFromForkLength(this->forkLength);
    float swimRange = swimSpeed*SECONDS_PER_TIMESTEP;
    // Loop until there are no new candidate nodes
    while (fringe.size() > 0) {
        // Pick a node off the fringe to evaluate
        std::tuple<MapNode *, float, float, float> popped = fringe.front();
        MapNode *point = std::get<0>(popped);
        float cost = std::get<1>(popped);
        float fitness = std::get<2>(popped);
        float probMass = std::get<3>(popped);
        // (remove the selected node)
        fringe.pop_front();
        // Check all channels flowing into this node
        std::vector<std::tuple<MapNode *, float, float>> neighbors;
        float elapsedTime = cost / swimSpeed;
        float remainingTime = SECONDS_PER_TIMESTEP - elapsedTime;
        float stayCost = remainingTime * model.hydroModel.getUnsignedFlowSpeedAt(*point);
        neighbors.emplace_back(point, cost+stayCost, fitness);
        for (Edge &edge : point->edgesIn) {
            if (!out.count(edge.source) && model.hydroModel.getDepth(*edge.source) >= MOVEMENT_DEPTH_CUTOFF) {
                // Effective movement speed along channel (swim speed adjusted by flow speed)
                float transitSpeed = swimSpeed - model.hydroModel.getFlowSpeedAlong(edge);
                // Check to make sure the fish can even make progress
                if (transitSpeed > 0.0f) {
                    // Calculate effective distance swum
                    float edgeCost = (edge.length/transitSpeed)*swimSpeed;
                    if (isDistributary(edge.source->type) && point == this->location){ // }  || (this->forkLength >= 75)) {
                        // Artificially discount the cost to make distributary nodes easier to access
                        // (since they are widely spaced)
                        // change to include discount if fork length > 75mm for exiting
                        edgeCost = std::min(edgeCost, swimRange - cost);
                    }
                    // Check if connected node is reachable
                    if (cost + edgeCost <= swimRange) {
                        // Add it to the fringe with the new total cost
                        float newFitness = this->getFitness(model, *edge.source, cost + edgeCost);
                        neighbors.emplace_back(edge.source, cost + edgeCost, newFitness);
                    }
                }
            }
        }
        // Check all channels flowing out of this node
        // Same as above (except flow directions are reversed)
        for (Edge &edge : point->edgesOut) {
            if (!out.count(edge.target) && model.hydroModel.getDepth(*edge.target) >= MOVEMENT_DEPTH_CUTOFF) {
                float transitSpeed = swimSpeed + model.hydroModel.getFlowSpeedAlong(edge);
                if (transitSpeed > 0.0f) {
                    float edgeCost = (edge.length/transitSpeed)*swimSpeed;
                    if (isDistributary(point->type) && point == this->location){ // || (this->forkLength >= 75)){
                        edgeCost = std::min(edgeCost, swimRange - cost);
                    }
                    if (cost + edgeCost <= swimRange) {
                        float newFitness = this->getFitness(model, *edge.target, cost + edgeCost);
                        neighbors.emplace_back(edge.target, cost + edgeCost, newFitness);
                    }
                }
            }
        }
        if (neighbors.size() == 0) {
            out[point] = (out.count(point) ? out[point] : 0.0f) + probMass;
        } else {
            float neighborFitnessSum = 0.0f;
            for (std::tuple<MapNode *, float, float> neighbor : neighbors) {
                neighborFitnessSum += std::get<2>(neighbor);
            }
            for (std::tuple<MapNode *, float, float> neighbor : neighbors) {
                if (std::get<0>(neighbor) == point) {
                    out[point] = (out.count(point) ? out[point] : 0.0f) + (std::get<2>(neighbor)/neighborFitnessSum) * probMass;
                } else {
                    fringe.emplace_back(
                        std::get<0>(neighbor),
                        std::get<1>(neighbor),
                        std::get<2>(neighbor),
                        (std::get<2>(neighbor)/neighborFitnessSum) * probMass
                    );
                }
            }
        }
    }
}

float Fish::getFitness(Model &model, MapNode &loc, float cost) {
    return this->getGrowth(model, loc, cost) / this->getMortality(model, loc);
}

void Fish::incrementExitHabitatHoursByOneTimestep() {
    this->numExitHabitatHours += HOURS_PER_TIMESTEP;
}

/*
 * Perform the movement simulation for this fish
 * (Correlated random walk terminating at a node with locally maximal fitness-value)
 */

bool Fish::move(Model &model) {
    float cost = 0.0f;
    float swimSpeed = swimSpeedFromForkLength(this->forkLength);
    float swimRange = swimSpeed*SECONDS_PER_TIMESTEP;
    float currFitness = this->getFitness(model, *this->location, 0.0f);
    MapNode *point = this->location;
    float lastFlowSpeed_node_old = model.hydroModel.getUnsignedFlowSpeedAt(*point);
    FlowVelocity lastFlowVelocity;
    std::vector<std::tuple<MapNode *, float, float>> neighbors;
    std::vector<float> weights;
    while (true) {
        neighbors.clear();
        float elapsedTime = cost / swimSpeed;
        float remainingTime = SECONDS_PER_TIMESTEP - elapsedTime;
        lastFlowVelocity = model.hydroModel.getScaledFlowVelocityAt(*point);

        float pointFlowSpeed = model.hydroModel.getUnsignedFlowSpeedAt(*point);
        float stayCost = remainingTime * pointFlowSpeed;
        if (remainingTime > 0.0f) {
            neighbors.emplace_back(point, cost+stayCost, currFitness);
            if (model.getInt(ModelParamKey::DirectionlessEdges)) {
                auto fishMovement = FishMovement(model);
                auto reachableNeighbors = fishMovement.getReachableNeighbors(
                    point,
                    swimSpeed,
                    swimRange,
                    cost,
                    this->location,
                    [this](Model& m, MapNode& node, float cost) { return this->getFitness(m, node, cost); }
                );
                neighbors.insert(
                    neighbors.end(),
                    std::make_move_iterator(reachableNeighbors.begin()),
                    std::make_move_iterator(reachableNeighbors.end())
                );
            }
            else {
                // Check all channels flowing into this node
                for (Edge &edge: point->edgesIn) {
                    if (model.hydroModel.getDepth(*edge.source) >= MOVEMENT_DEPTH_CUTOFF) {
                        float edgeFlowSpeed = model.hydroModel.getFlowSpeedAlong(edge);
                        // Effective movement speed along channel (swim speed adjusted by flow speed)
                        float transitSpeed = swimSpeed - edgeFlowSpeed;
                        // Check to make sure the fish can even make progress
                        if (transitSpeed > 0.0f) {
                            // Calculate effective distance swum
                            float edgeCost = (edge.length / transitSpeed) * swimSpeed;
                            if (isDistributary(edge.source->type) && point == this->location) {
                                // || (this->forkLength >= 75)){
                                // Artificially discount the cost to make at least 1 distributary channel passable
                                // (since they are widely spaced)
                                edgeCost = std::min(edgeCost, swimRange - cost);
                            }
                            // Check if connected node is reachable
                            if (cost + edgeCost <= swimRange) {
                                // Add it to the neighbor list with its fitness value
                                float fitness = this->getFitness(model, *edge.source, cost + edgeCost);
                                neighbors.emplace_back(edge.source, cost + edgeCost, fitness);
                            }
                        }
                    }
                }
                // Check all channels flowing out of this node
                // Same as above (except flow directions are reversed)
                for (Edge &edge: point->edgesOut) {
                    if (model.hydroModel.getDepth(*edge.target) >= MOVEMENT_DEPTH_CUTOFF) {
                        float edgeFlowSpeed = model.hydroModel.getFlowSpeedAlong(edge);
                        float transitSpeed = swimSpeed + edgeFlowSpeed;
                        if (transitSpeed > 0.0f) {
                            float edgeCost = (edge.length / transitSpeed) * swimSpeed;
                            // if (isDistributary(edge.target->type) && point == this->location) {
                            if (isDistributary(point->type) && point == this->location) {
                                //|| (this->forkLength >= 75)){
                                edgeCost = std::min(edgeCost, swimRange - cost);
                            }
                            if (cost + edgeCost <= swimRange) {
                                float fitness = this->getFitness(model, *edge.target, cost + edgeCost);
                                neighbors.emplace_back(edge.target, cost + edgeCost, fitness);
                            }
                        }
                    }
                }
            }
        }
        if (!neighbors.empty()) {
            weights.clear();
            float totalFitness = 0.0f;
            for (size_t i = 0; i < neighbors.size(); ++i) {
                totalFitness += std::get<2>(neighbors[i]);
            }
            for (size_t i = 0; i < neighbors.size(); ++i) {
                weights.emplace_back(std::get<2>(neighbors[i])/totalFitness);
            }
            size_t idx = sample(weights.data(), neighbors.size());
            MapNode *lastPoint = point;
            point = std::get<0>(neighbors[idx]);
            cost = std::get<1>(neighbors[idx]);
            currFitness = std::get<2>(neighbors[idx]);
            lastFlowVelocity = model.hydroModel.getScaledFlowVelocityAt(*point);

            if (point == lastPoint) {
                break;
            }
        } else {
            break;
        }
    }
    this->location = point;
    this->travel = cost;

    this->lastTemp = this->getBoundedTempForGrowth(model, *point);
    this->lastDepth = model.hydroModel.getDepth(*point);
    this->lastFlowSpeed_old = lastFlowSpeed_node_old;
    this->lastFlowVelocity = lastFlowVelocity;
    if (this->location->type == HabitatType::Nearshore) {
        this->incrementExitHabitatHoursByOneTimestep();
    } else {
        this->numExitHabitatHours = 0;
    }
    if (this->numExitHabitatHours >= model.habitatTypeExitConditionHours) {
        this->exit(model);
        return false;
    }

    /* 
    // simplistic approach to exiting fish when they reach 75mm length or less than 30
    if (this->forkLength >= 75 || this->forkLength < 30) {
        this->exit(model);
        return false;
    } 
    */
    
    if (model.hydroModel.getDepth(*this->location) <= 0.0f) {
        // Die from stranding if depth less than 0 (TODO re-evaluate this condition)
        this->dieStranding(model);
        return false;
    }
    return true;
}

/* OLD
 * Perform the movement simulation for this fish
 * (calculate reachable nodes, evaluate their fitness ratios, and sample a destination)
 */
 /* 
bool Fish::move(Model &model) {
    // Find reachable nodes
    std::unordered_map<MapNode *, float> reachable;
    this->getReachableNodes(model, reachable);
    // Arrays to hold sampling probability masses and associated nodes
    MapNode **dests = new MapNode*[reachable.size()];
    float *costs = new float[reachable.size()];
    float *weights = new float[reachable.size()];
    // Calculate each node's fitness ratio
    int i = 0;
    for (auto it = reachable.begin(); it != reachable.end(); ++it) {
        dests[i] = it->first;
        costs[i] = it->second;
        weights[i] = this->getGrowth(model, *(it->first), it->second)
            / this->getMortality(model, *(it->first));
        ++i;
    }
    // TODO: fish lacking destinations still?
    // Sample a destination node from the distribution formed by all reachable nodes' fitness ratios
    unsigned idx = sample(weights, reachable.size());
    // Update fish location and last travel distance fields
    this->location = dests[idx];
    this->travel = costs[idx];
    // Clean up arrays
    delete[] dests;
    delete[] costs;
    delete[] weights;
    if (this->location->type == HabitatType::Nearshore) {
        // Exit if at nearshore (TODO verify this behavior)
        this->exit(model);
        return false;
    }
    if (model.hydroModel.getDepth(*this->location) <= 0.0f) {
        // Die from stranding if depth less than 0 (TODO re-evaluate this condition)
        this->dieStranding(model);
        return false;
    }
    return true;
}
*/

// Mark this fish as having exited the model
void Fish::exit(Model &model) {
    this->status = FishStatus::Exited;
    this->exitTime = model.time;
}

// Mark this fish as having died due to mortality risk
void Fish::dieMortality(Model &model) {
    this->status = FishStatus::DeadMortality;
    this->exitTime = model.time;
}

// Mark this fish as having died due to stranding
void Fish::dieStranding(Model &model) {
    this->status = FishStatus::DeadStranding;
    this->exitTime = model.time;
}

// Mark this fish as having died due to insufficient consumption
void Fish::dieStarvation(Model &model) {
    this->status = FishStatus::DeadStarvation;
    this->exitTime = model.time;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
float Fish::getPmax(const Model &model, const MapNode &loc) { // NOLINT(*-convert-member-functions-to-static)
    constexpr float SQ_METER_TO_HECTARE_CONVERSION = 10000.0;
    const float growthSlope = model.getFloat(ModelParamKey::GrowthSlope);

    float Pmax = 0.8f - ((loc.popDensity * SQ_METER_TO_HECTARE_CONVERSION) * growthSlope);

    constexpr float PMAX_MIN = 0.2;
    if (Pmax < PMAX_MIN) {
        Pmax = PMAX_MIN;
    }
    if (isNearshore(loc.type)) {
        constexpr float PMAX_NEARSHORE = 1.0;
        Pmax = PMAX_NEARSHORE;
    }
    return Pmax;
}

float Fish::getBoundedTempForGrowth(Model &model, MapNode &loc) const {
    return fmin(CTM, model.hydroModel.getTemp(loc));
}

// Calculate growth amount (in g) for a given location and distance swum
// Cost is in meters
float Fish::getGrowth(Model &model, MapNode &loc, float cost) {
    const float pmax = this->getPmax(model, loc);
    return this->getGrowth(model, loc, cost, pmax);
}

float Fish::getGrowth(Model &model, MapNode &loc, float cost, float Pmax) const {
    const float my_temp = this->getBoundedTempForGrowth(model, loc);

    // TODO: Should fish die if temp > CTM? Otherwise have to cap temp
    const float V = (CTM - my_temp)/(CTM - CTO);
    const float fTcons = pow(V, CONS_X) * exp(CONS_X * (1 - V));
    const float Cmax = CA * pow(mass, CB);
    const float Consumption = Cmax * Pmax * fTcons;

    // Egestion and Excretion
    const float Egestion = FA * pow(my_temp, FB) * exp(FG * Pmax) * Consumption;
    const float Excretion = UA * pow(my_temp, UB) * exp(UG * Pmax) * (Consumption - Egestion);

    // Respiration (g*g^-1*d^-1)
    // cost is distance traveled this timestep, in m
    //TODO: if they are idling in a blind channel, do they swim around? Use standard vel?
    const float Velocity = (cost / (60*60)) * 100;  // Converting swim speed from m/s to cm/s
    //if my_temp > RTL:
    //  vel = RK1 * mass ** RK4
    //else:
    //	vel = ACT * mass ** RK4 * math.e ** (BACT * my_temp)
    const float Activity = exp(RTO * Velocity);
    const float fTresp = exp(RQ * my_temp);
    const float Respiration = RA * pow(mass, RB) * fTresp * Activity;
    const float SpecificDynamicAction = SDA * (Consumption - Egestion);

    // (g*g^-1*d^-1)
    const float Delta = Consumption - Respiration - SpecificDynamicAction - Egestion - Excretion;
    //96 timesteps a day -- 15min each
    const float Growth = (Delta / 24) * mass ;
    return Growth;
}

// Calculate mortality risk for a given node
float Fish::getMortality(Model &model, MapNode &loc) const {
    const double mort_min_c = model.getFloat(ModelParamKey::MortMin);
    const double mort_max_d = model.getFloat(ModelParamKey::MortMax);
    const float habitat_mortality_multiplier = model.getFloat(ModelParamKey::HabitatMortalityMultiplier);
    const double habTypeMortConst = habitatTypeMortalityConst(loc.type, habitat_mortality_multiplier);
    const double a = 1.849; // slope
    const double b_m = -0.8; //slope at inflection
    const double b_s = -2.395; // intercept
    const double e = 500; // inflection point on x
    const double L = this->forkLength;
    const double X = loc.popDensity; // * 1000; // convert m^2 to ha
    const double S = 250; // scaling factor numerator
    const double result = (((mort_min_c + (mort_max_d - mort_min_c) * exp(-exp(-b_m * (log(X) - log(e))))) * (S / (exp(b_s + a * log(L))) ))) * habTypeMortConst;
    return result;
}

// Calculate growth amount and mortality risk at this fish's current location,
// then apply growth and check mortality risk (and die if that's the way it goes)
bool Fish::growAndDie(Model &model) {
    const float pMax = this->getPmax(model, *(this->location));
    const float growth = this->getGrowth(model, *(this->location), this->travel, pMax);
    const float mortality = this->getMortality(model, *(this->location));
    this->lastGrowth = growth;
    this->lastPmax = pMax;
    this->lastMortality = mortality;

    this->trackHistory();

    this->mass = this->mass + growth;

    // check to make sure fish hasn't reached a critically low mass
    constexpr float MASS_MIN = 0.381;
    if (this->mass <= MASS_MIN) {
        this->dieStarvation(model);
        return false;
    }

    // Sample from bernoulli(m) to check if fish should die from mortality risk,
    const float mortalityProbability = mortality;
    float sample = unit_rand();
    if (sample <= mortalityProbability) {
        this->dieMortality(model);
        return false;
    }

    this->forkLength = forkLengthFromMass(this->mass);
    return true;
}

void Fish::addHistoryBuffers() {
    this->locationHistory = new std::vector<int>();
    this->growthHistory = new std::vector<float>();
    this->pmaxHistory = new std::vector<float>();
    this->mortalityHistory = new std::vector<float>();
    this->tempHistory = new std::vector<float>();
    this->depthHistory = new std::vector<float>();
    this->flowSpeedHistory_old = new std::vector<float>();
    this->flowVelocityHistory = new std::vector<FlowVelocity>();
}

void Fish::calculateMassHistory() {
    this->massHistory = new std::vector<float>();
    this->forkLengthHistory = new std::vector<float>();
    size_t T = this->locationHistory->size();
    this->massHistory->resize(T, 0.0f);
    this->forkLengthHistory->resize(T, 0.0f);
    for (size_t i = 0; i < T; ++i) {
        (*this->massHistory)[T - i - 1] = this->mass;
        (*this->forkLengthHistory)[T - i - 1] = forkLengthFromMass(this->mass);
        this->mass -= (*this->growthHistory)[T - i - 1];
    }
    this->mass = (*this->massHistory)[0];
    this->forkLength = (*this->forkLengthHistory)[0];
}

bool Fish::isNotTagged() const {
    return this->locationHistory == nullptr;
}

void Fish::trackHistory() const {
    if (isNotTagged()) {
        return;
    }
    if (this->location->type == HabitatType::Nearshore && this->lastPmax != 1.0) {
        std::cout << "Tracking nearshore Pmax: " << this->lastPmax << " for ID: " << this->location->id
        << " at step: " << this->locationHistory->size() -1 << std::endl;
    }
    this->locationHistory->push_back(this->location->id);
    this->growthHistory->push_back(this->lastGrowth);
    this->pmaxHistory->push_back(this->lastPmax);
    this->mortalityHistory->push_back(this->lastMortality);
    this->tempHistory->push_back(this->lastTemp);
    this->depthHistory->push_back(this->lastDepth);
    this->flowSpeedHistory_old->push_back(this->lastFlowSpeed_old);
    this->flowVelocityHistory->push_back(this->lastFlowVelocity);
}

void Fish::tag(Model &model) {
    constexpr int TAG_FREQUENCY = 2500;
    if (this->taggedTime != -1 || (this->id % TAG_FREQUENCY) != 0) {
        return;
    }
    this->addHistoryBuffers();
    this->taggedTime = model.time;
    this->trackHistory();
}
