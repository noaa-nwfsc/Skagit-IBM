#include "model.h"

#include <thread>
#include <algorithm>
#include "util.h"
#include "load.h"
#include "map_gen.h"
#include "env_sim.h"
#include <cstdio>
#include <fstream>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <stdexcept>
#include <netcdf>
#include <iostream>

// default mortality constants
constexpr float MORT_CONST_C = 0.03096;
constexpr float MORT_CONST_A = -0.42;
constexpr float DEFAULT_EXIT_CONDITION_HOURS = 2.0;

/*
 * Constructs a model instance from parameters and data filenames.
 * The model is initialized to be empty, and at timestep 0.
 * Loading saved states is handled by a separate function after construction. (Model::loadState)
 */
Model::Model(
    // Offset of this model's timestep 0 from midnight on January 1st (timestep 0 of the year)
    // (measured in timesteps)
    // Note: This is only used to determine the displayed date in the GUI
    int globalTimeIntercept,

    // The offset into the hydrological data corresponding to this model's first timestep
    // (measured in timesteps)
    int hydroTimeIntercept,
    // The offset into the recruitment data corresponding to this model's first timestep
    // (measured in timesteps)
    int recTimeIntercept,

    // The maximum number of threads to spawn for multithreaded computation of movement + growth/death
    size_t maxThreads,

    // Path of the file containing the daily recruitment counts
    std::string recCountFilename,
    // Path of the file containing the biweekly recruitment size distributions
    std::string recSizeDistsFilename,
    // List of map node IDs where new recruits enter the model
    std::vector<unsigned> recPointIds,
    float habitatTypeExitConditionHours,
    // Path of the file containing map node descriptions (area, habitat type, elevation, among other columns)
    std::string mapLocationFilename,
    // Path of the file containing map edge descriptions (source and target nodes, path lengths, among other columns)
    std::string mapEdgeFilename,
    // Path of the file containing mapping from node IDs to X/Y coords (in UTM zone 10N projection, if real map data; 1.0f = 1m)
    std::string mapGeometryFilename,
    // Radius within which blind channel nodes should be merged (in meters)
    float blindChannelSimplificationRadius,
    // Path of the crescent tide data (1 row per timestep)
    std::string cresTideFilename,
    // Path of the flow volume data (1 row per timestep)
    std::string flowVolFilename,
    // Path of the air temperature data (1 row per timestep)
    std::string airTempFilename,
    // Path of the flow speed data (netCDF)
    std::string flowSpeedFilename,
    // Path of the distributary WSE/temp data (netCDF)
    std::string distribWseTempFilename,
    const ModelConfigMap &config
) : defaultHydroModel(std::make_unique<HydroModel>(cresTideFilename, flowVolFilename, airTempFilename,
                                                   flowSpeedFilename, distribWseTempFilename, hydroTimeIntercept)),
    hydroModel(*defaultHydroModel),
    recTimeIntercept(recTimeIntercept),
    globalTimeIntercept(globalTimeIntercept),
    firstHighTide(false),
    time(0UL),
    deadCount(0),
    exitedCount(0),
    mortConstA(MORT_CONST_A),
    mortConstC(MORT_CONST_C),
    habitatTypeExitConditionHours(habitatTypeExitConditionHours),
    nextFishID(0UL),
    maxThreads(maxThreads),
    recruitTagRate(0.5f),
    configMap(config) {
    if (getInt(ModelParamKey::DirectionlessEdges)) std::cout << "directionless edges!" << std::endl;

    // Load the map
    loadMap(
        // The resulting nodes are stored in the model's "map" field
        this->map,
        mapLocationFilename,
        mapEdgeFilename,
        mapGeometryFilename,
        externalCsvIdToInternalId,
        hydroModel.hydroNodes,
        recPointIds,
        this->recPoints,
        this->monitoringPoints,
        this->samplingSites,
        blindChannelSimplificationRadius,
        configMap
    );
    for (size_t i = 0; i < this->monitoringPoints.size(); ++i) {
        this->monitoringHistory.emplace_back();
    }
    // Load the recruit counts, the data is stored in the model's "recCounts" field
    loadIntList(recCountFilename, this->recCounts);
    // Ditto for recruit sizes and sampling sites
    loadRecSizeDists(recSizeDistsFilename, this->recSizeDists);
    // Make room in the recruit plan vector (per-timestep recruit counts for the current day)
    this->recDayPlan.resize(24, 0UL);
}

// Load model components from simulated data (map & environmental conditions)
Model::Model(
    size_t maxThreads,
    std::vector<MapNode *> &map,
    std::vector<MapNode *> &recPoints,
    std::vector<int> &recCounts,
    std::vector<std::vector<float> > &recSizeDists,
    std::vector<std::vector<float> > &depths,
    std::vector<std::vector<float> > &temps,
    float distFlow
) : map(map),
    defaultHydroModel(std::make_unique<HydroModel>(map, depths, temps, distFlow)),
    hydroModel(*defaultHydroModel),
    recCounts(recCounts),
    recSizeDists(recSizeDists),
    recPoints(recPoints),
    recTimeIntercept(0),
    globalTimeIntercept(0),
    firstHighTide(false),
    time(0UL),
    deadCount(0),
    exitedCount(0),
    mortConstA(MORT_CONST_A),
    mortConstC(MORT_CONST_C),
    habitatTypeExitConditionHours(DEFAULT_EXIT_CONDITION_HOURS),
    nextFishID(0UL),
    maxThreads(maxThreads),
    recruitTagRate(0.5f) {
    // Make room in the recruit plan vector (per-timestep recruit counts for the current day)
    this->recDayPlan.resize(24, 0UL);
}

Model::Model(HydroModel *hydroModel)
    : defaultHydroModel(nullptr),
      hydroModel(*hydroModel),
      recTimeIntercept(0),
      globalTimeIntercept(0),
      firstHighTide(false),
      time(0UL),
      deadCount(0),
      exitedCount(0),
      mortConstA(MORT_CONST_A),
      mortConstC(MORT_CONST_C),
      habitatTypeExitConditionHours(DEFAULT_EXIT_CONDITION_HOURS),
      nextFishID(0UL),
      maxThreads(1),
      recruitTagRate(0.5f) {}

void Model::masterUpdate() {
    if (this->time % 24 == 0) {
        this->update24h();
    }
    if ((this->time / 24) % 14 == 0) {
        // On a sampling day
        // TODO add sampling parameters to config
        if (this->time % 24 == 12) {
            this->sampling();
        }
        // Currently all sites are treated as beach seine
        /*if (this->hydroModel.isHighTide() && firstHighTide) {
            this->firstHighTide = false;
            this->sampling(SamplingMode::Fyke);
        }*/
    }
    this->update1h();
    this->time += 1;
    // Sync the hydro model's time with the main model time
    this->hydroModel.updateTime(this->time);
}

void Model::update1h() {
    // Introduce new recruits
    this->recruit();
    // We aren't recalculating density between recruitment and movement since we want to turn a blind eye
    // to the recruit entry node bottleneck (by letting them move before counting, we pretend they don't bunch up)
    this->moveAll();
    // Calculate density, size distributions for each node to provide info needed for consumption/mortality calculations
    // The "false" here means the sampling trackers won't be updated (to avoid double-counting fish)
    this->countAll(false);
    this->growAndDieAll();
    // Recalculate densities to reflect mortality, this time with sampling tracking enabled
    this->countAll(true);
    // Add an entry to the population history
    this->populationHistory.push_back(this->livingIndividuals.size());
    // Record monitoring sites
    //this->checkMonitoringNodes(); // TODO: GROT
    for (size_t i = 0; i < this->monitoringPoints.size(); ++i) {
        MapNode *n = this->monitoringPoints[i];
        this->monitoringHistory[i].emplace_back(n->residentIds.size(), n->popDensity, hydroModel.getDepth(*n), hydroModel.getTemp(*n));
    }
}

// TODO: longer timestep, move based on current state, explore discretely? <-- think about this more

void Model::update24h() {
    // Generate the per-timestep recruit counts for the day
    this->planRecruitment();
    this->firstHighTide = true;
}

// Alias for an iterator of a list of fish IDs (position in the list)
typedef std::vector<size_t>::iterator FishIdIter;

// Run in each movement thread, processes movement for a subset of fish
void moveThread(
    Model *model,
    FishIdIter start,
    FishIdIter end
) {
    for (auto it = start; it != end; ++it) {
        model->individuals[*it].move(*model);
    }
}


// Handles launching of movement threads
void Model::moveAll() {
    // Each thread should handle at minimum 4096 fish
    unsigned threadBatchSize = std::max(4096U, (unsigned) (this->livingIndividuals.size() / this->maxThreads));
    // Figure out how many threads to launch based on the calculated per-thread fish count
    unsigned numThreads = std::max(1U, (unsigned) (this->livingIndividuals.size() / threadBatchSize));
    // Allocate storage for thread datastructures
    std::thread *threads = new std::thread[numThreads];
    // Iterator for the beginning of the living fish list
    auto start = this->livingIndividuals.begin();
    size_t remaining = this->livingIndividuals.size();

    for (unsigned i = 0; i < numThreads; ++i) {
        size_t batch = remaining / (numThreads - i); // How many fish are left to be processed
        remaining -= batch;
        // Iterator for the end of the current batch of fish to be processed
        auto end = start + batch;
        // Launch a thread
        threads[i] = std::thread(moveThread, this, start, end);
        // Shift the start point for the next batch to just past this batch's end
        start = end;
    }
    // Wait for all threads to finish running
    for (unsigned i = 0; i < numThreads; ++i) {
        threads[i].join();
    }
    // Free the thread storage (it was allocated on the heap)
    delete[] threads;

    // Re-pack the living fish into the first part of the living fish list

    // Tracker for where to put living fish in the list (start at the start)
    auto targetIt = this->livingIndividuals.begin();
    for (auto sourceIt = this->livingIndividuals.begin(); sourceIt != this->livingIndividuals.end(); ++sourceIt) {
        Fish &f = this->individuals[*sourceIt];
        // If a fish is alive, move it to the target tracker, then shift the target position over 1
        if (f.status == FishStatus::Alive) {
            *targetIt = *sourceIt;
            ++targetIt;
        } else if (f.status == FishStatus::Exited) {
            ++this->exitedCount;
        }
    }
    // Erase remaining (dead) fish
    if (targetIt != this->livingIndividuals.end()) {
        this->livingIndividuals.erase(targetIt, this->livingIndividuals.end());
    }
}

// Run in each growth+death thread, processes growth and death for a subset of fish
void growAndDieThread(
    Model *model,
    FishIdIter start,
    FishIdIter end
) {
    for (auto it = start; it != end; ++it) {
        model->individuals[*it].growAndDie(*model);
    }
}

// Handles launching of growth+death threads
void Model::growAndDieAll() {
    // Each thread should handle at minimum 4096 fish
    unsigned threadBatchSize = std::max(4096U, (unsigned) (this->livingIndividuals.size() / this->maxThreads));
    // Figure out how many threads to launch based on the calculated per-thread fish count
    unsigned numThreads = std::max(1U, (unsigned) (this->livingIndividuals.size() / threadBatchSize));
    // Allocate storage for thread datastructures
    std::thread *threads = new std::thread[numThreads];
    // Iterator for the beginning of the living fish list
    auto start = this->livingIndividuals.begin();
    size_t remaining = this->livingIndividuals.size();

    for (unsigned i = 0; i < numThreads; ++i) {
        size_t batch = remaining / (numThreads - i); // How many fish are left to be processed
        remaining -= batch;
        // Iterator for the end of the current batch of fish to be processed
        auto end = start + batch;
        // Launch a thread
        threads[i] = std::thread(growAndDieThread, this, start, end);
        // Shift the start point for the next batch to just past this batch's end
        start = end;
    }
    // Wait for all threads to finish
    for (unsigned i = 0; i < numThreads; ++i) {
        threads[i].join();
    }
    // Free thread storage
    delete[] threads;

    // Re-pack the living fish into the first part of the living fish list, remove dead fish

    // Tracker for where to put living fish in the list (start at the start)
    auto targetIt = this->livingIndividuals.begin();
    for (auto sourceIt = this->livingIndividuals.begin(); sourceIt != this->livingIndividuals.end(); ++sourceIt) {
        Fish &f = this->individuals[*sourceIt];
        // If a fish is alive, move it to the target tracker, then shift the target position over 1
        if (f.status == FishStatus::Alive) {
            *targetIt = *sourceIt;
            ++targetIt;
        } else if (f.status == FishStatus::Exited) {
            ++this->exitedCount;
        } else {
            ++this->deadCount;
        }
    }
    // Erase remaining (dead) fish
    if (targetIt != this->livingIndividuals.end()) {
        this->livingIndividuals.erase(targetIt, this->livingIndividuals.end());
    }
}

struct FishSortDummy {
    long id;
    float val;
    FishSortDummy(long id, float val) : id(id), val(val) {};

    friend bool operator<(const FishSortDummy &a, const FishSortDummy &b) {
        return a.val < b.val;
    }
};

// Calculate per-node population and median mass
void Model::countAll(bool updateTracking) {
    // Reset node tracker values
    for (MapNode *node: this->map) {
        node->residentIds.clear();
        node->maxMass = 0.0f;
    }
    // Place each fish in the trackers for its node
    for (long i: this->livingIndividuals) {
        Fish &f = this->individuals[i];
        f.location->residentIds.push_back(i);
        f.location->maxMass = std::max(f.location->maxMass, f.mass);
    }
    std::vector<FishSortDummy> residentMasses;
    std::vector<FishSortDummy> residentArrivalTimes;
    for (MapNode *node: this->map) {
        // Calculate population density (pop/area)
        node->popDensity = ((float) node->residentIds.size()) / node->area;
        // Calculate median mass
        if (node->residentIds.size() > 0) {
            residentMasses.clear();
            residentArrivalTimes.clear();
            // Make a list of node's resident masses
            for (long id: node->residentIds) {
                residentMasses.emplace_back(id, this->individuals[id].mass);
                residentArrivalTimes.emplace_back(id, this->individuals[id].travel);
            }
            // Set the median to the nth-largest element of the mass list, where n is half the length of the list
            std::sort(residentMasses.begin(), residentMasses.end());
            std::sort(residentArrivalTimes.begin(), residentArrivalTimes.end());
            for (size_t i = 0; i < residentMasses.size(); ++i) {
                this->individuals[residentMasses[i].id].massRank = i;
                this->individuals[residentArrivalTimes[i].id].arrivalTimeRank = residentMasses.size() - i - 1;
            }
        }
    }
}

// Generates a single recruit and adds it to a random recruit start node
void Model::recruitSingle() {
    // Get the current slice of the recruit size distribution data
    std::vector<float> &recSizeDist = this->recSizeDists[(this->time + this->recTimeIntercept) / (24 * 14)];
    // Sample the fork length bucket index from the distribution
    unsigned flIdx = sample(recSizeDist.data(), recSizeDist.size());
    // Calculate the fork length from the bucket index
    float forkLength = 35.0f + 5.0f * flIdx + unit_rand() * 5.0f;
    // Construct a fish and place it in the *ALL* fish list
    this->individuals.emplace_back(
        // This gets the new fish's ID (current val of nextFishID) and then updates nextFishID
        this->nextFishID++,
        this->time,
        forkLength,
        // This samples a random (uniform) recruit start node
        this->recPoints[GlobalRand::int_rand(0, (int) this->recPoints.size() - 1)]
    );
    // this->addHistoryBuffers();
    const size_t last_id = this->individuals.back().id;
    this->tagIndividual(last_id);
    // Place the new fish's ID in the living fish list
    this->livingIndividuals.push_back(last_id);
}

// Recruit all recruits for the current timestep
void Model::recruit() {
    // Get the current timestep's recruit count from the day's recruit "plan"
    size_t currRecCount = this->recDayPlan[this->time % 24];
    // Recruit that many fish
    for (size_t i = 0; i < currRecCount; ++i) {
        this->recruitSingle();
    }
}

// Generate the day's per-timestep recruit counts
void Model::planRecruitment() {
    // Wipe whatever's in the plan array right now
    for (size_t i = 0; i < 24; ++i) {
        this->recDayPlan[i] = 0;
    }
    // Get the day's daily recruit count
    size_t count = this->recCounts[(this->time + this->recTimeIntercept) / 24];
    // For each recruit in the day, place it in a random timestep's slot
    for (size_t i = 0; i < count; ++i) {
        size_t timestep = GlobalRand::int_rand(0, 23);
        ++this->recDayPlan[timestep];
    }
}

// Collect sampling data from the sampling sites
void Model::sampling() {
    for (SamplingSite *site: this->samplingSites) {
        // At each site, calculate the statistics of interest:
        float totalMass = 0.0f;
        float totalLength = 0.0f;
        int totalSpawnTime = 0;
        size_t totalPop = 0;
        // "Instant" sampling (just use the current timestep's resident info) for both modes
        // (difference is in how sampling nodes are assigned)
        for (MapNode *point: site->points) {
            for (long id: point->residentIds) {
                totalMass += this->individuals[id].mass;
                totalLength += this->individuals[id].forkLength;
                totalSpawnTime += this->individuals[id].spawnTime;
            }
            totalPop += point->residentIds.size();
        }
        float meanMass = totalPop > 0 ? totalMass / ((float) totalPop) : 0.0f;
        float meanLength = totalPop > 0 ? totalLength / ((float) totalPop) : 0.0f;
        float meanSpawnTime = totalPop > 0 ? ((float) totalSpawnTime) / ((float) totalPop) : 0.0f;
        // Create a sample data structure from the statistics
        Sample s{
            site->id,
            this->time,
            totalPop,
            meanMass,
            meanLength,
            meanSpawnTime
        };
        // Add the new sample to the sample history
        this->sampleHistory.push_back(s);
    }
}

// Destructor for the model (frees all model resources that aren't automatically freed)
Model::~Model() {
    for (MapNode *node: this->map) {
        delete node;
    }
    for (SamplingSite *site: this->samplingSites) {
        delete site;
    }
}

// Reset timestep to 0, clear all individual lists
void Model::reset() {
    this->time = 0L;
    this->hydroModel.updateTime(this->time);
    this->individuals.clear();
    this->livingIndividuals.clear();
    this->deadCount = 0;
    this->exitedCount = 0;
    this->populationHistory.clear();
    this->sampleHistory.clear();
    this->countAll(false);
}

// Save model state to a given filename
void Model::saveState(std::string savePath) {
    netCDF::NcFile targetFile(savePath, netCDF::NcFile::FileMode::replace);
    size_t N = this->individuals.size();
    // Add dimensions
    std::vector<netCDF::NcDim> noDims;
    netCDF::NcDim populationHistoryLength = targetFile.
            addDim("populationHistoryLength", this->populationHistory.size());
    std::vector<netCDF::NcDim> populationHistoryDims;
    populationHistoryDims.push_back(populationHistoryLength);
    netCDF::NcDim sampleHistoryLength = targetFile.addDim("sampleHistoryLength", this->sampleHistory.size());
    std::vector<netCDF::NcDim> sampleHistoryDims;
    sampleHistoryDims.push_back(sampleHistoryLength);
    netCDF::NcDim nDim = targetFile.addDim("n", N);
    std::vector<netCDF::NcDim> fishDims;
    fishDims.push_back(nDim);
    std::vector<netCDF::NcDim> monitoringDims;
    netCDF::NcDim monitoringPoints = targetFile.addDim("monitoringPoints", this->monitoringPoints.size());
    monitoringDims.push_back(monitoringPoints);
    monitoringDims.push_back(populationHistoryLength);
    std::vector<netCDF::NcDim> monitoringPointsDims;
    monitoringPointsDims.push_back(monitoringPoints);

    // Record model fields
    std::vector<size_t> noIndex;
    netCDF::NcVar modelTime = targetFile.addVar("modelTime", netCDF::ncInt, noDims);
    modelTime.putVar(noIndex, this->time);

    // Record fish
    int *recruitTimeOut = new int[N];
    int *exitTimeOut = new int[N];
    float *entryForkLengthOut = new float[N];
    float *entryMassOut = new float[N];
    float *forkLengthOut = new float[N];
    float *massOut = new float[N];
    int *statusOut = new int[N];
    int *locationOut = new int[N];
    float *travelOut = new float[N];
    float *lastGrowthOut = new float[N];
    float *lastPmaxOut = new float[N];
    float *lastMortalityOut = new float[N];
    float *lastTempOut = new float[N];
    float *lastDepthOut = new float[N];
    float *lastFlowSpeedOut = new float[N];
    float *lastFlowVelocityUOut = new float[N];
    float *lastFlowVelocityVOut = new float[N];
    for (size_t n = 0; n < N; ++n) {
        Fish &f = this->individuals[n];
        recruitTimeOut[n] = f.spawnTime;
        exitTimeOut[n] = f.exitTime;
        entryForkLengthOut[n] = f.entryForkLength;
        entryMassOut[n] = f.entryMass;
        forkLengthOut[n] = f.forkLength;
        massOut[n] = f.mass;
        statusOut[n] = (int) f.status;
        locationOut[n] = f.location->id;
        travelOut[n] = f.travel;
        lastGrowthOut[n] = f.lastGrowth;
        lastPmaxOut[n] = f.lastPmax;
        lastMortalityOut[n] = f.lastMortality;
        lastTempOut[n] = f.lastTemp;
        lastDepthOut[n] = f.lastDepth;
        lastFlowSpeedOut[n] = f.lastFlowSpeed_old;
        lastFlowVelocityUOut[n] = f.lastFlowVelocity.u;
        lastFlowVelocityVOut[n] = f.lastFlowVelocity.v;
    }
    netCDF::NcVar recruitTime = targetFile.addVar("recruitTime", netCDF::ncInt, fishDims);
    recruitTime.putVar(recruitTimeOut);
    netCDF::NcVar exitTime = targetFile.addVar("exitTime", netCDF::ncInt, fishDims);
    exitTime.putVar(exitTimeOut);
    netCDF::NcVar entryForkLength = targetFile.addVar("entryForkLength", netCDF::ncFloat, fishDims);
    entryForkLength.putVar(entryForkLengthOut);
    netCDF::NcVar entryMass = targetFile.addVar("entryMass", netCDF::ncFloat, fishDims);
    entryMass.putVar(entryMassOut);
    netCDF::NcVar forkLength = targetFile.addVar("forkLength", netCDF::ncFloat, fishDims);
    forkLength.putVar(forkLengthOut);
    netCDF::NcVar mass = targetFile.addVar("mass", netCDF::ncFloat, fishDims);
    mass.putVar(massOut);
    netCDF::NcVar status = targetFile.addVar("status", netCDF::ncInt, fishDims);
    status.putVar(statusOut);
    netCDF::NcVar location = targetFile.addVar("location", netCDF::ncInt, fishDims);
    location.putVar(locationOut);
    netCDF::NcVar travel = targetFile.addVar("travel", netCDF::ncFloat, fishDims);
    travel.putVar(travelOut);
    netCDF::NcVar lastGrowth = targetFile.addVar("lastGrowth", netCDF::ncFloat, fishDims);
    lastGrowth.putVar(lastGrowthOut);
    netCDF::NcVar lastPmax = targetFile.addVar("lastPmax", netCDF::ncFloat, fishDims);
    lastPmax.putVar(lastPmaxOut);
    netCDF::NcVar lastMortality = targetFile.addVar("lastMortality", netCDF::ncFloat, fishDims);
    lastMortality.putVar(lastMortalityOut);
    netCDF::NcVar lastTemp = targetFile.addVar("lastTemp", netCDF::ncFloat, fishDims);
    lastTemp.putVar(lastTempOut);
    netCDF::NcVar lastDepth = targetFile.addVar("lastDepth", netCDF::ncFloat, fishDims);
    lastDepth.putVar(lastDepthOut);
    netCDF::NcVar lastFlowSpeed = targetFile.addVar("lastFlowSpeed", netCDF::ncFloat, fishDims);
    lastFlowSpeed.putVar(lastFlowSpeedOut);
    netCDF::NcVar lastVelocityU = targetFile.addVar("lastFlowVelocityU", netCDF::ncFloat, fishDims);
    lastVelocityU.putVar(lastFlowVelocityUOut);
    netCDF::NcVar lastVelocityV = targetFile.addVar("lastFlowVelocityV", netCDF::ncFloat, fishDims);
    lastVelocityV.putVar(lastFlowVelocityVOut);

    // Write population history
    netCDF::NcVar populationHistoryVar = targetFile.addVar("populationHistory", netCDF::ncInt, populationHistoryDims);
    populationHistoryVar.putVar(this->populationHistory.data());

    // Write sample history
    int *sampleSiteIDOut = new int[this->sampleHistory.size()];
    int *sampleTimeOut = new int[this->sampleHistory.size()];
    int *samplePopOut = new int[this->sampleHistory.size()];
    float *sampleMeanMassOut = new float[this->sampleHistory.size()];
    float *sampleMeanLengthOut = new float[this->sampleHistory.size()];
    float *sampleMeanSpawnTimeOut = new float[this->sampleHistory.size()];
    for (size_t i = 0; i < this->sampleHistory.size(); ++i) {
        sampleSiteIDOut[i] = this->sampleHistory[i].siteID;
        sampleTimeOut[i] = this->sampleHistory[i].time;
        samplePopOut[i] = this->sampleHistory[i].population;
        sampleMeanMassOut[i] = this->sampleHistory[i].meanMass;
        sampleMeanLengthOut[i] = this->sampleHistory[i].meanLength;
        sampleMeanSpawnTimeOut[i] = this->sampleHistory[i].meanSpawnTime;
    }
    netCDF::NcVar sampleSiteID = targetFile.addVar("sampleSiteID", netCDF::ncInt, sampleHistoryDims);
    sampleSiteID.putVar(sampleSiteIDOut);
    netCDF::NcVar sampleTime = targetFile.addVar("sampleTime", netCDF::ncInt, sampleHistoryDims);
    sampleTime.putVar(sampleTimeOut);
    netCDF::NcVar samplePop = targetFile.addVar("samplePop", netCDF::ncInt, sampleHistoryDims);
    samplePop.putVar(samplePopOut);
    netCDF::NcVar sampleMeanMass = targetFile.addVar("sampleMeanMass", netCDF::ncFloat, sampleHistoryDims);
    sampleMeanMass.putVar(sampleMeanMassOut);
    netCDF::NcVar sampleMeanLength = targetFile.addVar("sampleMeanLength", netCDF::ncFloat, sampleHistoryDims);
    sampleMeanLength.putVar(sampleMeanLengthOut);
    netCDF::NcVar sampleMeanSpawnTime = targetFile.addVar("sampleMeanSpawnTime", netCDF::ncFloat, sampleHistoryDims);
    sampleMeanSpawnTime.putVar(sampleMeanSpawnTimeOut);

    int *monitoringPopulationOut = new int[this->monitoringPoints.size() * this->populationHistory.size()];
    float *monitoringPopulationDensityOut = new float[this->monitoringPoints.size() * this->populationHistory.size()];
    float *monitoringDepthOut = new float[this->monitoringPoints.size() * this->populationHistory.size()];
    float *monitoringTempOut = new float[this->monitoringPoints.size() * this->populationHistory.size()];
    int *monitoringPointsOut = new int[this->monitoringPoints.size()];
    for (size_t i = 0; i < this->monitoringPoints.size(); ++i) {
        monitoringPointsOut[i] = this->monitoringPoints[i]->id;
        for (size_t t = 0; t < this->populationHistory.size(); ++t) {
            monitoringPopulationOut[i * this->populationHistory.size() + t] = this->monitoringHistory[i][t].population;
            monitoringPopulationDensityOut[i * this->populationHistory.size() + t] = this->monitoringHistory[i][t].populationDensity;
            monitoringDepthOut[i * this->populationHistory.size() + t] = this->monitoringHistory[i][t].depth;
            monitoringTempOut[i * this->populationHistory.size() + t] = this->monitoringHistory[i][t].temp;
        }
    }

    netCDF::NcVar monitoringPopulation = targetFile.addVar("monitoringPopulation", netCDF::ncInt, monitoringDims);
    monitoringPopulation.putVar(monitoringPopulationOut);
    netCDF::NcVar monitoringPopulationDensity = targetFile.addVar("monitoringPopulationDensity", netCDF::ncFloat, monitoringDims);
    monitoringPopulationDensity.putVar(monitoringPopulationDensityOut);
    netCDF::NcVar monitoringDepth = targetFile.addVar("monitoringDepth", netCDF::ncFloat, monitoringDims);
    monitoringDepth.putVar(monitoringDepthOut);
    netCDF::NcVar monitoringTemp = targetFile.addVar("monitoringTemp", netCDF::ncFloat, monitoringDims);
    monitoringTemp.putVar(monitoringTempOut);
    netCDF::NcVar monitoringPointIDs = targetFile.addVar("monitoringPointIDs", netCDF::ncInt, monitoringPointsDims);
    monitoringPointIDs.putVar(monitoringPointsOut);
}

// Load model state from a given filename
// Returns true if the loading was successful, false if something went wrong
void Model::loadState(std::string loadPath) {
    netCDF::NcFile sourceFile(loadPath, netCDF::NcFile::FileMode::read);
    size_t N = sourceFile.getDim("n").getSize();
    long recruitTimeDummy;
    float forkLengthDummy;
    long locationDummy;
    int statusDummy;
    netCDF::NcVar recruitTime = sourceFile.getVar("recruitTime");
    netCDF::NcVar exitTime = sourceFile.getVar("exitTime");
    netCDF::NcVar entryForkLength = sourceFile.getVar("entryForkLength");
    netCDF::NcVar entryMass = sourceFile.getVar("entryMass");
    netCDF::NcVar forkLength = sourceFile.getVar("forkLength");
    netCDF::NcVar mass = sourceFile.getVar("mass");
    netCDF::NcVar status = sourceFile.getVar("status");
    netCDF::NcVar location = sourceFile.getVar("location");
    netCDF::NcVar travel = sourceFile.getVar("travel");
    netCDF::NcVar lastGrowth = sourceFile.getVar("lastGrowth");
    netCDF::NcVar lastPmax = sourceFile.getVar("lastPmax");
    netCDF::NcVar lastMortality = sourceFile.getVar("lastMortality");
    netCDF::NcVar lastTemp = sourceFile.getVar("lastTemp");
    netCDF::NcVar lastDepth = sourceFile.getVar("lastDepth");
    netCDF::NcVar lastFlowSpeed = sourceFile.getVar("lastFlowSpeed");
    netCDF::NcVar lastFlowVelocityU = sourceFile.getVar("lastFlowVelocityU");
    netCDF::NcVar lastFlowVelocityV = sourceFile.getVar("lastFlowVelocityV");
    this->individuals.clear();
    std::vector<size_t> idxVec{0U};
    for (size_t id = 0; id < N; ++id) {
        idxVec[0] = id;
        recruitTime.getVar(idxVec, &recruitTimeDummy);
        forkLength.getVar(idxVec, &forkLengthDummy);
        location.getVar(idxVec, &locationDummy);
        this->individuals.emplace_back(id, recruitTimeDummy, forkLengthDummy, this->map[locationDummy]);
        Fish &f = this->individuals[id];
        exitTime.getVar(idxVec, &f.exitTime);
        entryForkLength.getVar(idxVec, &f.entryForkLength);
        entryMass.getVar(idxVec, &f.entryMass);
        mass.getVar(idxVec, &f.mass);
        status.getVar(idxVec, &statusDummy);
        f.status = (FishStatus) statusDummy;
        travel.getVar(idxVec, &f.travel);
        lastGrowth.getVar(idxVec, &f.lastGrowth);
        lastPmax.getVar(idxVec, &f.lastPmax);
        lastMortality.getVar(idxVec, &f.lastMortality);
        lastTemp.getVar(idxVec, &f.lastTemp);
        lastDepth.getVar(idxVec, &f.lastDepth);
        lastFlowSpeed.getVar(idxVec, &f.lastFlowSpeed_old);
        lastFlowVelocityU.getVar(idxVec, &f.lastFlowVelocity.u);
        lastFlowVelocityV.getVar(idxVec, &f.lastFlowVelocity.v);
    }

    this->populationHistory.clear();
    size_t populationHistoryLength = sourceFile.getDim("populationHistoryLength").getSize();
    this->populationHistory.resize(populationHistoryLength);
    sourceFile.getVar("populationHistory").getVar(this->populationHistory.data());

    this->sampleHistory.clear();
    size_t sampleHistoryLength = sourceFile.getDim("sampleHistoryLength").getSize();
    int sampleSiteIDDummy;
    int sampleTimeDummy;
    int samplePopDummy;
    float sampleMeanMassDummy;
    float sampleMeanLengthDummy;
    float sampleMeanSpawnTimeDummy;
    netCDF::NcVar sampleSiteID = sourceFile.getVar("sampleSiteID");
    netCDF::NcVar sampleTime = sourceFile.getVar("sampleTime");
    netCDF::NcVar samplePop = sourceFile.getVar("samplePop");
    netCDF::NcVar sampleMeanMass = sourceFile.getVar("sampleMeanMass");
    netCDF::NcVar sampleMeanLength = sourceFile.getVar("sampleMeanLength");
    netCDF::NcVar sampleMeanSpawnTime = sourceFile.getVar("sampleMeanSpawnTime");
    for (size_t i = 0; i < sampleHistoryLength; ++i) {
        idxVec[0] = i;
        sampleSiteID.getVar(idxVec, &sampleSiteIDDummy);
        sampleTime.getVar(idxVec, &sampleTimeDummy);
        samplePop.getVar(idxVec, &samplePopDummy);
        sampleMeanMass.getVar(idxVec, &sampleMeanMassDummy);
        sampleMeanLength.getVar(idxVec, &sampleMeanLengthDummy);
        sampleMeanSpawnTime.getVar(idxVec, &sampleMeanSpawnTimeDummy);
        this->sampleHistory.emplace_back((size_t) sampleSiteIDDummy, sampleTimeDummy, (size_t) samplePopDummy,
                                         sampleMeanMassDummy, sampleMeanLengthDummy, sampleMeanSpawnTimeDummy);
    }

    this->monitoringHistory.clear();
    this->monitoringPoints.clear();
    size_t numMonitoringPoints = sourceFile.getDim("monitoringPoints").getSize();
    std::vector<size_t> idxVec2{0U, 0U};
    netCDF::NcVar monitoringPointIDs = sourceFile.getVar("monitoringPointIDs");
    netCDF::NcVar monitoringPopulation = sourceFile.getVar("monitoringPopulation");
    netCDF::NcVar monitoringPopulationDensity = sourceFile.getVar("monitoringPopulationDensity");
    netCDF::NcVar monitoringDepth = sourceFile.getVar("monitoringDepth");
    netCDF::NcVar monitoringTemp = sourceFile.getVar("monitoringTemp");
    int monitoringPointIDDummy;
    int monitoringPopulationDummy;
    float monitoringPopulationDensityDummy;
    float monitoringDepthDummy;
    float monitoringTempDummy;
    for (size_t i = 0; i < numMonitoringPoints; ++i) {
        idxVec2[0] = i;
        idxVec[0] = i;
        monitoringPointIDs.getVar(idxVec, &monitoringPointIDDummy);
        this->monitoringPoints.push_back(this->map[monitoringPointIDDummy]);
        this->monitoringHistory.emplace_back();
        for (size_t t = 0; t < populationHistoryLength; ++t) {
            idxVec2[1] = t;
            monitoringPopulation.getVar(idxVec2, &monitoringPopulationDummy);
            monitoringPopulationDensity.getVar(idxVec2, &monitoringPopulationDensityDummy);
            monitoringDepth.getVar(idxVec2, &monitoringDepthDummy);
            monitoringTemp.getVar(idxVec2, &monitoringTempDummy);
            this->monitoringHistory[i].emplace_back((size_t)
                monitoringPopulationDummy, monitoringPopulationDensityDummy, monitoringDepthDummy, monitoringTempDummy);
        }
    }

    // Set up the tracker values (density & current recruit plan) that weren't saved
    this->planRecruitment();
    this->countAll(false);
}


// Write a summary of all individuals' vital statistics to the provided filename
void Model::saveSummary(std::string savePath) {
    netCDF::NcFile targetFile(savePath, netCDF::NcFile::FileMode::replace);
    size_t N = this->individuals.size();
    int *recruitTimeOut = new int[N];
    int *exitTimeOut = new int[N];
    float *entryForkLengthOut = new float[N];
    float *entryMassOut = new float[N];
    float *finalForkLengthOut = new float[N];
    float *finalMassOut = new float[N];
    int *finalStatusOut = new int[N];
    for (size_t n = 0; n < N; ++n) {
        Fish &f = this->individuals[n];
        recruitTimeOut[n] = f.spawnTime;
        exitTimeOut[n] = f.exitTime;
        entryForkLengthOut[n] = f.entryForkLength;
        entryMassOut[n] = f.entryMass;
        finalForkLengthOut[n] = f.forkLength;
        finalMassOut[n] = f.mass;
        finalStatusOut[n] = (int) f.status;
    }
    netCDF::NcDim nDim = targetFile.addDim("n", N);
    std::vector<netCDF::NcDim> dims;
    dims.push_back(nDim);
    std::vector<netCDF::NcDim> monitoringDims;
    netCDF::NcDim monitoringPoints = targetFile.addDim("monitoringPoints", this->monitoringPoints.size());
    netCDF::NcDim historyLength = targetFile.addDim("historyLength", this->populationHistory.size());
    monitoringDims.push_back(monitoringPoints);
    monitoringDims.push_back(historyLength);
    std::vector<netCDF::NcDim> monitoringPointsDims;
    monitoringPointsDims.push_back(monitoringPoints);

    netCDF::NcVar recruitTime = targetFile.addVar("recruitTime", netCDF::ncInt, dims);
    recruitTime.putVar(recruitTimeOut);
    netCDF::NcVar exitTime = targetFile.addVar("exitTime", netCDF::ncInt, dims);
    exitTime.putVar(exitTimeOut);
    netCDF::NcVar entryForkLength = targetFile.addVar("entryForkLength", netCDF::ncFloat, dims);
    entryForkLength.putVar(entryForkLengthOut);
    netCDF::NcVar entryMass = targetFile.addVar("entryMass", netCDF::ncFloat, dims);
    entryMass.putVar(entryMassOut);
    netCDF::NcVar finalForkLength = targetFile.addVar("finalForkLength", netCDF::ncFloat, dims);
    finalForkLength.putVar(finalForkLengthOut);
    netCDF::NcVar finalMass = targetFile.addVar("finalMass", netCDF::ncFloat, dims);
    finalMass.putVar(finalMassOut);
    netCDF::NcVar finalStatus = targetFile.addVar("finalStatus", netCDF::ncInt, dims);
    finalStatus.putVar(finalStatusOut);

    int *monitoringPopulationOut = new int[this->monitoringPoints.size() * this->populationHistory.size()];
    float *monitoringPopulationDensityOut = new float[this->monitoringPoints.size() * this->populationHistory.size()];
    float *monitoringDepthOut = new float[this->monitoringPoints.size() * this->populationHistory.size()];
    float *monitoringTempOut = new float[this->monitoringPoints.size() * this->populationHistory.size()];
    int *monitoringPointsOut = new int[this->monitoringPoints.size()];
    for (size_t i = 0; i < this->monitoringPoints.size(); ++i) {
        monitoringPointsOut[i] = this->monitoringPoints[i]->id;
        for (size_t t = 0; t < this->populationHistory.size(); ++t) {
            monitoringPopulationOut[i * this->populationHistory.size() + t] = this->monitoringHistory[i][t].population;
            monitoringPopulationDensityOut[i * this->populationHistory.size() + t] = this->monitoringHistory[i][t].populationDensity;
            monitoringDepthOut[i * this->populationHistory.size() + t] = this->monitoringHistory[i][t].depth;
            monitoringTempOut[i * this->populationHistory.size() + t] = this->monitoringHistory[i][t].temp;
        }
    }

    netCDF::NcVar monitoringPopulation = targetFile.addVar("monitoringPopulation", netCDF::ncInt, monitoringDims);
    monitoringPopulation.putVar(monitoringPopulationOut);
    netCDF::NcVar monitoringPopulationDensity = targetFile.addVar("monitoringPopulationDensity", netCDF::ncFloat, monitoringDims);
    monitoringPopulationDensity.putVar(monitoringPopulationDensityOut);
    netCDF::NcVar monitoringDepth = targetFile.addVar("monitoringDepth", netCDF::ncFloat, monitoringDims);
    monitoringDepth.putVar(monitoringDepthOut);
    netCDF::NcVar monitoringTemp = targetFile.addVar("monitoringTemp", netCDF::ncFloat, monitoringDims);
    monitoringTemp.putVar(monitoringTempOut);
    netCDF::NcVar monitoringPointIDs = targetFile.addVar("monitoringPointIDs", netCDF::ncInt, monitoringPointsDims);
    monitoringPointIDs.putVar(monitoringPointsOut);
}

void Model::saveSampleData(std::string savePath) {
    netCDF::NcFile targetFile(savePath, netCDF::NcFile::FileMode::replace);
    // Add dimensions
    std::vector<netCDF::NcDim> noDims;
    netCDF::NcDim sampleHistoryLength = targetFile.addDim("sampleHistoryLength", this->sampleHistory.size());
    std::vector<netCDF::NcDim> sampleHistoryDims;
    sampleHistoryDims.push_back(sampleHistoryLength);

    // Write sample history
    int *sampleSiteIDOut = new int[this->sampleHistory.size()];
    int *sampleTimeOut = new int[this->sampleHistory.size()];
    int *samplePopOut = new int[this->sampleHistory.size()];
    float *sampleMeanMassOut = new float[this->sampleHistory.size()];
    float *sampleMeanLengthOut = new float[this->sampleHistory.size()];
    float *sampleMeanSpawnTimeOut = new float[this->sampleHistory.size()];
    for (size_t i = 0; i < this->sampleHistory.size(); ++i) {
        sampleSiteIDOut[i] = this->sampleHistory[i].siteID;
        sampleTimeOut[i] = this->sampleHistory[i].time;
        samplePopOut[i] = this->sampleHistory[i].population;
        sampleMeanMassOut[i] = this->sampleHistory[i].meanMass;
        sampleMeanLengthOut[i] = this->sampleHistory[i].meanLength;
        sampleMeanSpawnTimeOut[i] = this->sampleHistory[i].meanSpawnTime;
    }
    netCDF::NcVar sampleSiteID = targetFile.addVar("sampleSiteID", netCDF::ncInt, sampleHistoryDims);
    sampleSiteID.putVar(sampleSiteIDOut);
    netCDF::NcVar sampleTime = targetFile.addVar("sampleTime", netCDF::ncInt, sampleHistoryDims);
    sampleTime.putVar(sampleTimeOut);
    netCDF::NcVar samplePop = targetFile.addVar("samplePop", netCDF::ncInt, sampleHistoryDims);
    samplePop.putVar(samplePopOut);
    netCDF::NcVar sampleMeanMass = targetFile.addVar("sampleMeanMass", netCDF::ncFloat, sampleHistoryDims);
    sampleMeanMass.putVar(sampleMeanMassOut);
    netCDF::NcVar sampleMeanLength = targetFile.addVar("sampleMeanLength", netCDF::ncFloat, sampleHistoryDims);
    sampleMeanLength.putVar(sampleMeanLengthOut);
    netCDF::NcVar sampleMeanSpawnTime = targetFile.addVar("sampleMeanSpawnTime", netCDF::ncFloat, sampleHistoryDims);
    sampleMeanSpawnTime.putVar(sampleMeanSpawnTimeOut);
}

void Model::saveNodeIdMapping(const std::string &nodeIdMappingPath) {
    try {
        netCDF::NcFile targetFile(nodeIdMappingPath, netCDF::NcFile::FileMode::replace);

        std::vector<unsigned int> keys;
        std::vector<unsigned int> values;

        keys.reserve(externalCsvIdToInternalId.size());
        values.reserve(externalCsvIdToInternalId.size());

        for (const auto &pair: externalCsvIdToInternalId) {
            keys.push_back(pair.first);
            values.push_back(pair.second);
        }

        netCDF::NcDim mapSizeDim = targetFile.addDim("map_size", keys.size());

        netCDF::NcVar externalIds = targetFile.addVar("externalNodeIds", netCDF::ncUint, mapSizeDim);
        netCDF::NcVar internalIds = targetFile.addVar("internalNodeIds", netCDF::ncUint, mapSizeDim);

        externalIds.putVar(keys.data());
        internalIds.putVar(values.data());
    } catch (netCDF::exceptions::NcException &e) {
        std::cerr << "NetCDF error: " << e.what() << std::endl;
        throw;
    }
}

void Model::saveHydroMapping(const std::string &hydroMappingCsvPath) const {
    std::ofstream hydroMapOutFile(hydroMappingCsvPath);
    if (!hydroMapOutFile) {
        std::cerr << "Error opening file for writing: " << hydroMappingCsvPath << std::endl;
    }
    std::ostringstream headerLine;
    headerLine << "internal_node_ID" << "," << "hydro_node_ID" << "," << "distance";
    hydroMapOutFile << headerLine.str() << std::endl;

    for (const MapNode *node: map) {
        std::ostringstream lineStream;
        lineStream << node->id << ", " << node->nearestHydroNodeID << ", " << node->hydroNodeDistance;
        hydroMapOutFile << lineStream.str() << std::endl;
    }

    hydroMapOutFile.close();
}

// Set the proportion of recruits that should be tagged for full life history recording
void Model::setRecruitTagRate(float rate) { this->recruitTagRate = rate; }

// Tag an individual so that its full life history is recorded
void Model::tagIndividual(const size_t id) { this->individuals[id].tag(*this); }


// Write the full life histories for tagged individuals to the provided filename
void Model::saveTaggedHistories(std::string savePath) {
    std::cout << "In saveTaggedHistories" << std::endl;

    netCDF::NcFile targetFile(savePath, netCDF::NcFile::FileMode::replace);

    std::vector<size_t> taggedFish;
    for (size_t i = 0; i < this->individuals.size(); ++i) {
        // std::cout << std::endl << "individual taggedTime: " << this->individuals[i].taggedTime << std::endl;
        if (this->individuals[i].taggedTime != -1L) {
            taggedFish.push_back(i);
        }
    }
    size_t N = taggedFish.size();
    long T = this->time + 1;
    std::cout << std::endl << "Values for N: " << N << ", and T: " << T << std::endl;
    int *recruitTimeOut = new int[N];
    int *taggedTimeOut = new int[N];
    int *exitTimeOut = new int[N];
    float *entryForkLengthOut = new float[N];
    float *entryMassOut = new float[N];
    float *finalForkLengthOut = new float[N];
    float *finalMassOut = new float[N];
    int *finalStatusOut = new int[N];
    int *locationHistoryOut = new int[N * T];
    float *growthHistoryOut = new float[N * T];
    float *pmaxHistoryOut = new float[N * T];
    float *mortalityHistoryOut = new float[N * T];
    float *tempHistoryOut = new float[N * T];
    float *depthHistoryOut = new float[N * T];
    float *flowSpeedHistoryOut = new float[N * T];
    float *flowVelocityUHistoryOut = new float[N * T];
    float *flowVelocityVHistoryOut = new float[N * T];
    std::cout << std::endl << "N: " << N << ",   T: " << T << std::endl;

    for (size_t n = 0; n < N; ++n) {
        Fish &f = this->individuals[taggedFish[n]];
        // std::cout << std::endl << "loaded Fish: " << n << std::endl;
        recruitTimeOut[n] = f.spawnTime;
        // std::cout << std::endl << "set spawnTime: " << recruitTimeOut[n] << std::endl;
        taggedTimeOut[n] = f.taggedTime;
        exitTimeOut[n] = f.exitTime;
        entryForkLengthOut[n] = f.entryForkLength;
        entryMassOut[n] = f.entryMass;
        finalForkLengthOut[n] = f.forkLength;
        finalMassOut[n] = f.mass;
        finalStatusOut[n] = (int) f.status;
        // std::cout << std::endl << "finished loading Fish: " << n << std::endl;
        for (long t = 0; t < T; ++t) {
            // std::cout << std::endl << "Starting Time Steps with t = " << t << std::endl;
            // std::cout << std::endl << "Calculated [n*T+t] as: " << n*T+t << std::endl;
            // std::cout << std::endl << "f.taggedTime: " << f.taggedTime << std::endl;
            // std::cout << std::endl << "f.locationHistory->size(): " << f.locationHistory->size() << std::endl;
            // std::cout << std::endl << "f.growthHistory->size(): " << f.growthHistory->size() << std::endl;
            // std::cout << std::endl << "f.mortalityHistory->size(): " << f.mortalityHistory->size() << std::endl;
            if (t < f.taggedTime || t >= f.taggedTime + (long) f.locationHistory->size()) {
                // std::cout << std::endl << "Condition 1.1" << std::endl;
                locationHistoryOut[n * T + t] = -1;
            } else {
                // std::cout << std::endl << "Condition 1.2" << std::endl;
                locationHistoryOut[n * T + t] = (*f.locationHistory)[t - f.taggedTime];
            }

            // std::cout << std::endl << "Condition 2.2" << std::endl;
            bool outsideTaggedRange = t < f.taggedTime || t >= f.taggedTime + (long) f.growthHistory->size();
            growthHistoryOut[n * T + t] = outsideTaggedRange ? 0.0f : (*f.growthHistory)[t - f.taggedTime];
            pmaxHistoryOut[n * T + t] = outsideTaggedRange ? 0.0f : (*f.pmaxHistory)[t - f.taggedTime];
            mortalityHistoryOut[n * T + t] = outsideTaggedRange ? 0.0f : (*f.mortalityHistory)[t - f.taggedTime];
            tempHistoryOut[n * T + t] = outsideTaggedRange ? 0.0f : (*f.tempHistory)[t - f.taggedTime];
            depthHistoryOut[n * T + t] = outsideTaggedRange ? 0.0f : (*f.depthHistory)[t - f.taggedTime];
            flowSpeedHistoryOut[n * T + t] = outsideTaggedRange ? 0.0f : (*f.flowSpeedHistory_old)[t - f.taggedTime];
            flowVelocityUHistoryOut[n * T + t] =
                    outsideTaggedRange ? 0.0 : (*f.flowVelocityHistory)[t - f.taggedTime].u;
            flowVelocityVHistoryOut[n * T + t] =
                    outsideTaggedRange ? 0.0 : (*f.flowVelocityHistory)[t - f.taggedTime].v;
        }
        // std::cout << std::endl << "finished loading all T data: " << T << std::endl;
    }
    std::cout << std::endl << "Finished loading all tracks" << std::endl;
    netCDF::NcDim nDim = targetFile.addDim("n", N);
    netCDF::NcDim tDim = targetFile.addDim("t", T);
    std::vector<netCDF::NcDim> dimsN;
    std::vector<netCDF::NcDim> dimsNT;
    dimsN.push_back(nDim);
    dimsNT.push_back(nDim);
    dimsNT.push_back(tDim);

    netCDF::NcVar recruitTime = targetFile.addVar("recruitTime", netCDF::ncInt, dimsN);
    recruitTime.putVar(recruitTimeOut);
    netCDF::NcVar taggedTime = targetFile.addVar("taggedTime", netCDF::ncInt, dimsN);
    taggedTime.putVar(taggedTimeOut);
    netCDF::NcVar exitTime = targetFile.addVar("exitTime", netCDF::ncInt, dimsN);
    exitTime.putVar(exitTimeOut);
    netCDF::NcVar entryForkLength = targetFile.addVar("entryForkLength", netCDF::ncFloat, dimsN);
    entryForkLength.putVar(entryForkLengthOut);
    netCDF::NcVar entryMass = targetFile.addVar("entryMass", netCDF::ncFloat, dimsN);
    entryMass.putVar(entryMassOut);
    netCDF::NcVar finalForkLength = targetFile.addVar("finalForkLength", netCDF::ncFloat, dimsN);
    finalForkLength.putVar(finalForkLengthOut);
    netCDF::NcVar finalMass = targetFile.addVar("finalMass", netCDF::ncFloat, dimsN);
    finalMass.putVar(finalMassOut);
    netCDF::NcVar finalStatus = targetFile.addVar("finalStatus", netCDF::ncInt, dimsN);
    finalStatus.putVar(finalStatusOut);
    netCDF::NcVar locationHistory = targetFile.addVar("locationHistory", netCDF::ncInt, dimsNT);
    locationHistory.putVar(locationHistoryOut);
    netCDF::NcVar growthHistory = targetFile.addVar("growthHistory", netCDF::ncFloat, dimsNT);
    growthHistory.putVar(growthHistoryOut);
    netCDF::NcVar pmaxHistory = targetFile.addVar("pmaxHistory", netCDF::ncFloat, dimsNT);
    pmaxHistory.putVar(pmaxHistoryOut);
    netCDF::NcVar mortalityHistory = targetFile.addVar("mortalityHistory", netCDF::ncFloat, dimsNT);
    mortalityHistory.putVar(mortalityHistoryOut);
    netCDF::NcVar tempHistory = targetFile.addVar("tempHistory", netCDF::ncFloat, dimsNT);
    tempHistory.putVar(tempHistoryOut);
    netCDF::NcVar depthHistory = targetFile.addVar("depthHistory", netCDF::ncFloat, dimsNT);
    depthHistory.putVar(depthHistoryOut);
    netCDF::NcVar flowSpeedHistory = targetFile.addVar("flowSpeedHistory", netCDF::ncFloat, dimsNT);
    flowSpeedHistory.putVar(flowSpeedHistoryOut);
    netCDF::NcVar flowVelocityUHistory = targetFile.addVar("flowVelocityUHistory", netCDF::ncFloat, dimsNT);
    flowVelocityUHistory.putVar(flowVelocityUHistoryOut);
    netCDF::NcVar flowVelocityVHistory = targetFile.addVar("flowVelocityVHistory", netCDF::ncFloat, dimsNT);
    flowVelocityVHistory.putVar(flowVelocityVHistoryOut);
}

void Model::loadTaggedHistories(std::string loadPath) {
    this->reset();
    netCDF::NcFile sourceFile(loadPath, netCDF::NcFile::FileMode::read);
    size_t N = sourceFile.getDim("n").getSize();
    long T = sourceFile.getDim("t").getSize();
    long recruitTimeDummy;
    float forkLengthDummy;
    long locationDummy;
    int statusDummy;
    float growthDummy;
    float pmaxDummy;
    float mortalityDummy;
    float tempDummy;
    float depthDummy;
    float flowSpeedDummy;
    FlowVelocity flowVelocityDummy;
    netCDF::NcVar recruitTime = sourceFile.getVar("recruitTime");
    netCDF::NcVar taggedTime = sourceFile.getVar("taggedTime");
    netCDF::NcVar exitTime = sourceFile.getVar("exitTime");
    netCDF::NcVar entryForkLength = sourceFile.getVar("entryForkLength");
    netCDF::NcVar entryMass = sourceFile.getVar("entryMass");
    netCDF::NcVar finalForkLength = sourceFile.getVar("finalForkLength");
    netCDF::NcVar finalMass = sourceFile.getVar("finalMass");
    netCDF::NcVar finalStatus = sourceFile.getVar("finalStatus");
    netCDF::NcVar locationHistory = sourceFile.getVar("locationHistory");
    netCDF::NcVar growthHistory = sourceFile.getVar("growthHistory");
    netCDF::NcVar pmaxHistory = sourceFile.getVar("pmaxHistory");
    netCDF::NcVar mortalityHistory = sourceFile.getVar("mortalityHistory");
    netCDF::NcVar tempHistory = sourceFile.getVar("tempHistory");
    netCDF::NcVar depthHistory = sourceFile.getVar("depthHistory");
    netCDF::NcVar flowSpeedHistory = sourceFile.getVar("flowSpeedHistory");
    netCDF::NcVar flowVelocityUHistory = sourceFile.getVar("flowVelocityUHistory");
    netCDF::NcVar flowVelocityVHistory = sourceFile.getVar("flowVelocityVHistory");
    this->individuals.clear();
    std::vector<size_t> idxVecN{0U};
    std::vector<size_t> idxVecNT{0U, 0U};
    for (size_t id = 0; id < N; ++id) {
        idxVecN[0] = id;
        idxVecNT[0] = id;
        idxVecNT[1] = T - 1;
        recruitTime.getVar(idxVecN, &recruitTimeDummy);
        finalForkLength.getVar(idxVecN, &forkLengthDummy);
        finalStatus.getVar(idxVecN, &statusDummy);
        locationHistory.getVar(idxVecNT, &locationDummy);
        this->individuals.emplace_back(id, recruitTimeDummy, forkLengthDummy, this->map[locationDummy]);
        Fish &f = this->individuals[id];
        taggedTime.getVar(idxVecN, &f.taggedTime);
        exitTime.getVar(idxVecN, &f.exitTime);
        entryForkLength.getVar(idxVecN, &f.entryForkLength);
        entryMass.getVar(idxVecN, &f.entryMass);
        finalMass.getVar(idxVecN, &f.mass);
        f.exitStatus = (FishStatus) statusDummy;
        f.addHistoryBuffers();
        for (long t = f.taggedTime; t < T; ++t) {
            idxVecNT[1] = t;
            locationHistory.getVar(idxVecNT, &locationDummy);
            if (locationDummy == -1L) {
                break;
            }
            growthHistory.getVar(idxVecNT, &growthDummy);
            pmaxHistory.getVar(idxVecNT, &pmaxDummy);
            mortalityHistory.getVar(idxVecNT, &mortalityDummy);
            tempHistory.getVar(idxVecNT, &tempDummy);
            depthHistory.getVar(idxVecNT, &depthDummy);
            flowSpeedHistory.getVar(idxVecNT, &flowSpeedDummy);
            flowVelocityUHistory.getVar(idxVecNT, &flowVelocityDummy.u);
            flowVelocityVHistory.getVar(idxVecNT, &flowVelocityDummy.v);
            f.locationHistory->push_back(locationDummy);
            f.growthHistory->push_back(growthDummy);
            f.pmaxHistory->push_back(pmaxDummy);
            f.mortalityHistory->push_back(mortalityDummy);
            f.tempHistory->push_back(tempDummy);
            f.depthHistory->push_back(depthDummy);
            f.flowSpeedHistory_old->push_back(flowSpeedDummy);
            f.flowVelocityHistory->push_back(flowVelocityDummy);
        }
        f.calculateMassHistory();
    }
}

void Model::setHistoryTimestep(long timestep) {
    this->time = timestep;
    this->hydroModel.updateTime(timestep);
    this->livingIndividuals.clear();
    for (Fish &f: this->individuals) {
        if (timestep >= f.taggedTime && timestep < f.taggedTime + (long) f.locationHistory->size()) {
            f.location = this->map[(*f.locationHistory)[timestep - f.taggedTime]];
            f.lastGrowth = (*f.growthHistory)[timestep - f.taggedTime];
            f.lastPmax = (*f.pmaxHistory)[timestep - f.taggedTime];
            f.lastMortality = (*f.mortalityHistory)[timestep - f.taggedTime];
            f.lastTemp = (*f.tempHistory)[timestep - f.taggedTime];
            f.lastDepth = (*f.depthHistory)[timestep - f.taggedTime];
            f.lastFlowSpeed_old = (*f.flowSpeedHistory_old)[timestep - f.taggedTime];
            f.lastFlowVelocity = (*f.flowVelocityHistory)[timestep - f.taggedTime];
            f.status = FishStatus::Alive;
            f.mass = (*f.massHistory)[timestep - f.taggedTime];
            f.forkLength = (*f.forkLengthHistory)[timestep - f.taggedTime];
            this->livingIndividuals.push_back(f.id);
        } else if (timestep >= f.exitTime) {
            f.status = f.exitStatus;
        }
    }
    this->countAll(false);
}

int Model::getInt(ModelParamKey key) const {
    return configMap.getInt(key);
}

float Model::getFloat(ModelParamKey key) const {
    return configMap.getFloat(key);
}

std::string Model::getString(ModelParamKey key) const {
    return configMap.getString(key);
}

// Initialize a model instance from a JSON config file
Model *modelFromConfig(std::string configPath) {
    FILE *fp = fopen(configPath.c_str(), "r");
    // Bail out if the config file can't be loaded
    if (fp == nullptr)
        throw std::runtime_error(std::string("Could not open config file") + configPath);
    // Set up the JSON reader
    char readBuf[65536];
    rapidjson::FileReadStream is(fp, readBuf, sizeof(readBuf));
    rapidjson::Document d;
    d.ParseStream(is);

    ModelConfigMap config;
    config.loadFromJson(d);

    unsigned int rng_seed = config.getInt(ModelParamKey::rng_seed);
    GlobalRand::reseed(rng_seed);
    std::string envDataType = d["envDataType"].GetString();
    unsigned int hwThreads = std::thread::hardware_concurrency();
    std::cout << hwThreads << " hardware threads available" << std::endl;
    int desiredThreads = -1;
    if (d.HasMember("threadCount")) {
        desiredThreads = d["threadCount"].GetInt();
    }
    if (rng_seed != GlobalRand::USE_RANDOM_SEED) {
        desiredThreads = 1;
        std::cout << "Forcing max threads to 1 due to non-zero RNG seed (" << rng_seed << ")" << std::endl;
    }
    if (desiredThreads <= 0) {
        desiredThreads = hwThreads;
    }
    size_t maxThreads = std::max(1U, std::min(hwThreads, (unsigned int) desiredThreads));
    std::cout << maxThreads << " thread(s) enabled" << std::endl;
    Model *m;
    if (envDataType == "file") {
        // Get model params from JSON config object
        int recStartTime = d["recStartTimestep"].GetInt();
        int hydroStartTime = d["hydroStartTimestep"].GetInt();
        // calculate the model start date as the later of the hydro/recruit data
        int timeIntercept = std::max(recStartTime, hydroStartTime);
        int recTimeIntercept = timeIntercept - recStartTime;
        int hydroTimeIntercept = timeIntercept - hydroStartTime;
        // load the JSON list of entry nodes into a CPP vector
        std::vector<unsigned> recPoints;
        rapidjson::Value &recPointArr = d["recruitEntryNodes"];
        for (rapidjson::Value::ConstValueIterator it = recPointArr.Begin(); it != recPointArr.End(); ++it) {
            recPoints.push_back(it->GetUint());
        }

        m = new Model(
            timeIntercept,
            hydroTimeIntercept,
            recTimeIntercept,
            maxThreads,
            std::string(d["recruitCountsFile"].GetString()),
            std::string(d["recruitSizesFile"].GetString()),
            recPoints,
            d.HasMember("habitatTypeExitConditionHours")
                ? d["habitatTypeExitConditionHours"].GetFloat()
                : DEFAULT_EXIT_CONDITION_HOURS,
            std::string(d["mapNodesFile"].GetString()),
            std::string(d["mapEdgesFile"].GetString()),
            std::string(d["mapGeometryFile"].GetString()),
            d.HasMember("blindChannelSimplificationRadius")
                ? d["blindChannelSimplificationRadius"].GetFloat()
                : 0.0f,
            std::string(d["tideFile"].GetString()),
            std::string(d["flowVolFile"].GetString()),
            std::string(d["airTempFile"].GetString()),
            std::string(d["flowSpeedFile"].GetString()),
            std::string(d["distribWseTempFile"].GetString()),
            config
        );
    } else {
        // Generate map from JSON config params
        std::vector<MapNode *> map;
        std::vector<MapNode *> recPoints;
        generateMap(
            map, recPoints,
            d["mapParams"]["m"].GetInt(),
            d["mapParams"]["n"].GetInt(),
            d["mapParams"]["a"].GetFloat(),
            d["mapParams"]["pDist"].GetFloat(),
            d["mapParams"]["pBlind"].GetFloat()
        );
        int simLength = d["simLength"].GetInt();
        std::vector<std::vector<float> > depths;
        std::vector<std::vector<float> > temps;
        float distFlow;
        env_sim(
            simLength,
            map,
            depths,
            temps,
            distFlow
        );
        std::vector<int> recCounts;
        std::vector<std::vector<float> > recSizeDists;
        float lambda = d["recruitRate"].GetFloat();
        float meanSize = d["recruitSizeMean"].GetFloat();
        float sizeStd = d["recruitSizeStd"].GetFloat();
        for (int day = 0; day <= simLength / 24; ++day) {
            recCounts.push_back(poisson(lambda));
        }
        std::vector<float> recSizeDist;
        for (int bucketIdx = 0; bucketIdx < 14; ++bucketIdx) {
            double bucketSize = 35.0 + 5.0 * (double) bucketIdx;
            recSizeDist.push_back(normal_pdf(bucketSize, meanSize, sizeStd));
        }
        for (int week = 0; week <= simLength / (24 * 7); ++week) {
            recSizeDists.push_back(recSizeDist);
        }
        m = new Model(
            maxThreads,
            map,
            recPoints,
            recCounts, recSizeDists,
            depths, temps, distFlow
        );
    }
    fclose(fp);
    return m;
}
