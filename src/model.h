#ifndef __FISH_MODEL_H
#define __FISH_MODEL_H

#include <string>
#include <unordered_map>
#include <vector>
#include "fish.h"
#include "map.h"
#include "hydro.h"

#ifndef __FISH_FISH_CLS
class Fish;
#endif


// This struct represents the results of a single biweekly sampling instance at a given sampling site
typedef struct Sample {
    // The site's ID
    size_t siteID;
    // The model timestep at which the sample was taken
    long time;
    // The sample population
    size_t population;
    // The mean mass of sampled fish (g)
    float meanMass;
    // The mean fork length of sampled fish (mm)
    float meanLength;
    //float meanLogLength;
    // The mean spawn timestep of sampled fish
    float meanSpawnTime;
    Sample(size_t siteID, long time, size_t population, float meanMass, float meanLength, float meanSpawnTime)
        : siteID(siteID), time(time), population(population), meanMass(meanMass), meanLength(meanLength), meanSpawnTime(meanSpawnTime) {}
} Sample;

typedef struct MonitoringRecord {
    size_t population;
    float depth;
    float temp;
    MonitoringRecord(size_t population, float depth, float temp) : population(population), depth(depth), temp(temp) {}
} MonitoringRecord;

class Model {
public:
    // List of heap-allocated map locations
    std::vector<MapNode *> map;
    // The hydrology model
    HydroModel hydroModel;

    // Loaded daily recruit counts (see CONFIG_README for file format)
    std::vector<int> recCounts;
    // Loaded weekly recruit size distributions (see CONFIG_README for file format)
    std::vector<std::vector<float>> recSizeDists;
    // Map locations at which recruits are added
    std::vector<MapNode *> recPoints;
    // A list of per-timestep recruit counts, resampled once per day such that sum(recDayPlan) == recCounts[day]
    std::vector<size_t> recDayPlan;
    // The list of SamplingSite structs that determine where and how biweekly sampling is conducted (see SamplingSite in map.h)
    std::vector<SamplingSite *> samplingSites;
    // List of locations for which to track population/environmental values per timestep
    std::vector<MapNode *> monitoringPoints;
    // Timesteps between midnight on Jan 1 and the start of the recruitment data
    int recTimeIntercept;
    // Timesteps between midnight on Jan 1 and the timestep considered by the model to be timestep 0
    int globalTimeIntercept;
    // Whether or not a high tide has already occurred since the start of the current day
    bool firstHighTide;

    // The current timestep
    long time;
    // The list containing all Fish instances, living, dead, and exited
    std::vector<Fish> individuals;
    // The list containing currently active fish
    std::vector<size_t> livingIndividuals;
    // The number of fish that have died so far
    int deadCount;
    // The number of fish that have left the model without dying so far
    int exitedCount;
    // The per-timestep record of living population
    std::vector<int> populationHistory;
    // The list of biweekly sampling results
    std::vector<Sample> sampleHistory;
    // The list of per-timestep populations and environmental values for each monitoring point
    std::vector<std::vector<MonitoringRecord>> monitoringHistory;

    // Mortality constants overridden by ABC
    float mortConstA;
    float mortConstC;
    Model(
        int globalTimeIntercept,
        int hydroTimeIntercept,
        int recTimeIntercept,
        size_t maxThreads,
        std::string recCountFilename,
        std::string recSizeDistsFilename,
        std::vector<unsigned> recPointIds,
        std::string mapLocationFilename,
        std::string mapEdgeFilename,
        std::string mapGeometryFilename,
        float blindChannelSimplificationRadius,
        std::string cresTideFilename,
        std::string flowFilename,
        std::string airTempFilename,
        std::string flowSpeedFilename,
        std::string distribWseTempFilename
    );

    Model(
        size_t maxThreads,
        std::vector<MapNode *> &map,
        std::vector<MapNode *> &recPoints,
        std::vector<int> &recCounts,
        std::vector<std::vector<float>> &recSizeDists,
        std::vector<std::vector<float>> &depths,
        std::vector<std::vector<float>> &temps,
        float distFlow
    );

    // Call to advance the model state by one timestep
    void masterUpdate();
    // Wraps update procedures that happen every timestep
    void update1h();
    // Wraps update procedures that happen daily
    void update24h();
    // Calls Fish::move for every living fish and removes fish that die during this procedure from livingIndividuals
    void moveAll();
    // Computes local population statistics, including density, median and mean mass for each location
    void countAll(bool updateTracking);
    // Calls Fish::grow for every living fish and removes fish that die during this procedure from livingIndividuals
    void growAndDieAll();
    // Generates and adds new fish according to the current timestep's entry in recDayPlan
    void recruit();
    // Generates and adds a single new fish
    void recruitSingle();
    // Resamples recDayPlan to determine per-timestep recruit counts for the next day
    void planRecruitment();
    // Computes sampling results and adds new entries to samplingHistory
    void sampling();
    // Resets the model state
    void reset();
    // Saves model state to the provided filename
    void saveState(std::string savePath);
    // Loads model state from the provided filename
    void loadState(std::string loadPath);
    // Write a summary of all individuals' vital statistics to the provided filename
    void saveSummary(std::string savePath);
    // Write all sampling results to the provided filename
    void saveSampleData(std::string savePath);
    // Set the proportion of recruits that should be tagged for full life history recording
    void setRecruitTagRate(float rate);
    // Tag an individual so that its full life history is recorded
    void tagIndividual(size_t id);
    // Write the full life histories for tagged individuals to the provided filename
    void saveTaggedHistories(std::string savePath);
    // Read individuals' life histories saved by saveTaggedHistories into the "individuals" list
    // so that their histories can be replayed in the GUI
    void loadTaggedHistories(std::string loadPath);
    // Set the model's timestep and update fish to reflect the data
    // in the currently loaded life histories
    void setHistoryTimestep(long timestep);
    
    // add addhistory from fish???
    // void addHistoryBuffers();
    ~Model();
private:

    unsigned long nextFishID;
    size_t maxThreads;
    float recruitTagRate;

};
#define __FISH_MODEL_CLS

Model *modelFromConfig(std::string configPath);

#endif
