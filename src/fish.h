#ifndef __FISH_FISH_H
#define __FISH_FISH_H

#include <unordered_map>

#include "model.h"
#include "map.h"

/*
* Alive: currently active (this fish is in Model::livingIndividuals)
* DeadMortality: died during Model::growAndDieAll
*   as a result of mortality risk
* DeadStranding: died during Model::moveAll
*   as a result of being trapped in a dry location
* DeadStarvation: died during Model::growAndDieAll
*   as a result of consistent negative growth
* Exited: No longer in Model::livingIndividuals
*   as a result of reaching an exit node
*/
enum class FishStatus {Alive, DeadMortality, DeadStranding, DeadStarvation, Exited};

#ifndef __FISH_MODEL_CLS
class Model;
#endif

class Fish {
public:
    // index in Model::individuals
    unsigned long id;
    // timestep when this fish was recruited
    long spawnTime;
    // timestep when this fish reached an exit node or died
    long exitTime;
    // fork length upon model entry (mm)
    float entryForkLength;
    // mass upon model entry (g)
    float entryMass;
    // fork length (mm)
    float forkLength;
    // mass (g)
    float mass;
    // pointer to current map location
    MapNode *location;
    // meters this fish travelled last timestep
    float travel;
    // status; see FishStatus declaration above
    FishStatus status;
    // status on model exit; only used for keeping track of fish replays
    FishStatus exitStatus;
    // last timestep's growth (g) and Pmax
    float lastGrowth;
    float lastPmax;
    // last timestep's mortality (probability value)
    float lastMortality;
    // the mass rank of this fish among fish at its location
    int massRank;
    // the arrival time rank of this fish among fish at its location
    int arrivalTimeRank;
    // when this fish was tagged (-1 if it's not a tagged fish)
    long taggedTime;
    // location history (only present if tagged)
    std::vector<int> *locationHistory;
    // Pmax history (only present if tagged)
    std::vector<float> *pmaxHistory;
    // growth history (only present if tagged)
    std::vector<float> *growthHistory;
    // mortality history (only present if tagged)
    std::vector<float> *mortalityHistory;
    // mass history (only present if in replay mode)
    std::vector<float> *massHistory;
    // fork length history (only present if in replay mode)
    std::vector<float> *forkLengthHistory;

    Fish(
        unsigned long id,
        long spawnTime,
        float forkLength,
        MapNode *location
    );

    /*
    * Populate 'out' with a mapping from reachable map locations
    * to the associated travel cost (meters swum)
    */
    void getReachableNodes(Model &model, std::unordered_map<MapNode *, float> &out);
    /*
    * Populate 'out' with a mapping from reachable map locations to the
    * probability of arriving at the location
    */
    void getDestinationProbs(Model &model, std::unordered_map<MapNode *, float> &out);
    /*
    * Run this fish's movement update:
    *   Get all reachable nodes and their costs,
    *   then sample a destination weighted by its
    *   expected growth/mortality ratio and move there
    * Returns true if this fish is alive post-update
    */
    bool move(Model &model);
    // Register this fish as exited
    void exit(Model &model);
    // Register this fish as dead due to mortality risk
    void dieMortality(Model &model);
    // Register this fish as dead due to stranding
    void dieStranding(Model &model);
    // Register this fish as dead due to starvation
    void dieStarvation(Model &model);
    float getPmax(const MapNode &loc);
    // Compute the growth (g) for a given location and movement cost (meters swum) and pmax
    float getGrowth(Model &model, MapNode &loc, float cost, float pmax);
    // compute growth, pmax calculated internally
    float getGrowth(Model &model, MapNode &loc, float cost);

    // Compute the expected mortality risk for a given location
    float getMortality(Model &model, MapNode &loc);
    // Compute the ratio of growth to mortality for a given location and movement cost
    float getFitness(Model &model, MapNode &loc, float cost);
    /*
    * Run this fish's growth update:
    *   Get the growth (g) and mortality risk for the current location
    *   Optionally die due to mortality risk or starvation
    *   Update fish mass and fork length
    * Returns true if this fish is alive post-update
    */
    bool growAndDie(Model &model);
    // Create lists to keep track of vital rates and location
    void addHistoryBuffers();
    // Back-calculate mass and fork length histories from growth and final mass
    void calculateMassHistory();
    // Mark this fish as "tagged" (so that its full life history will be recorded)
    void tag(Model &model);
};
#define __FISH_FISH_CLS

#endif