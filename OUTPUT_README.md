
## Output Files

The model is capable of producing three different kinds of output files:

### State Snapshots

State snapshots can be saved in two ways:

- Pressing Ctrl+C to interrupt the headless model, then entering the 'save' command (with the desired filename)
- In the GUI, selecting File -> Save from the menu

State snapshots can be loaded from the GUI using the File -> Load menu item in order to load the saved state of the model.

### Sample data

Sample data files are produced by the `headless` executable, and consist of a record of statistics of population samples taken
at various sampling sites throughout the map. This data is also included in state snapshots.
Sample data is saved as `output_X.nc`, where X is the run ID.

### Summaries

Summaries are automatically saved as `summary_X.nc` by the headless executable when a simulation is finished, where X is the run ID.

### Tagged Fish Histories

Tagged fish histories can be saved from the GUI using the File -> Save Tagged History menu item. They can also be loaded in the GUI using File -> Load Tagged History in order to view a replay of the history of the tagged fish.

## Formats

All model outputs are saved as [netCDF 4](https://www.unidata.ucar.edu/software/netcdf/) files.

### State Snapshots

- Snapshot files contain the following dimensions:
    - `n`: Indicates individual, for per-fish variables
    - `populationHistoryLength`: Indicates timestep for population history entries
    - `sampleHistoryLength`: Indicates sample number for sample history entries (not equivalent to time)

- Snapshot files contain the following variables:
    - `modelTime`: int, the current model timestep
    - `recruitTime[n]`: ints, each fish's entry timestep
    - `exitTime[n]`: ints, each fish's exit or death timestep (only valid for a given `n` if `status[n]` is not 0, since status 0 is Alive)
    - `entryForkLength[n]`: floats, fork length of each fish upon entry (mm)
    - `entryMass[n]`: floats, mass of each fish upon entry (g)
    - `forkLength[n]`: floats, current/final fork length of each fish (mm)
    - `mass[n]`: floats, current/final mass of each fish (g)
    - `status[n]`: ints, current/final status of each fish
        - 0: Alive
        - 1: Dead from mortality risk
        - 2: Dead from stranding
        - 3: Dead from starvation
        - 4: Exited
    - `location[n]`: ints, current/final map node ID where each fish is located
    - `travel[n]`: floats, most recent timestep's swim distance for each fish (m)
    - `lastGrowth[n]`: floats, most recent timestep's growth for each fish (g)
    - `lastPmax[n]`: floats, most recent timestep's Pmax for each fish (p)
    - `lastMortality[n]`: floats, most recent timestep's mortality risk for each fish (probability value)
    - `populationHistory[populationHistoryLength]`: ints, per-timestep counts of living individuals in the model
    - `sampleSiteID[sampleHistoryLength]`: ints, each sample's site ID
        - Refer to [data/sampling_sites.csv](data/sampling_sites.csv) for a list of sample site names and coordinates. Site IDs correspond to line numbers (site ID 0 is Grain of Sand, 1 is FWP New Site, etc.)
    - `sampleTime[sampleHistoryLength]`: ints, the timestep when each sample was taken
    - `samplePop[sampleHistoryLength]`: ints, the number of individuals in each sample
    - `sampleMeanMass[sampleHistoryLength]`: floats, each sample's mean individual mass (g)
    - `sampleMeanLength[sampleHistoryLength]`: floats, each sample's mean individual fork length (mm)
    - `sampleMeanSpawnTime[sampleHistoryLength]`: floats, each sample's mean individual spawn time (timesteps)

### Sample data

- Snapshot files contain the following dimensions:
    - `sampleHistoryLength`: Indicates sample number for sample history entries (not equivalent to time)
- Sample data files contain the following variables:
    - `sampleSiteID[sampleHistoryLength]`: ints, each sample's site ID
        - Refer to [data/sampling_sites.csv](data/sampling_sites.csv) for a list of sample site names and coordinates. Site IDs correspond to line numbers (site ID 0 is Grain of Sand, 1 is FWP New Site, etc.)
    - `sampleTime[sampleHistoryLength]`: ints, the timestep when each sample was taken
    - `samplePop[sampleHistoryLength]`: ints, the number of individuals in each sample
    - `sampleMeanMass[sampleHistoryLength]`: floats, each sample's mean individual mass (g)
    - `sampleMeanLength[sampleHistoryLength]`: floats, each sample's mean individual fork length (mm)
    - `sampleMeanSpawnTime[sampleHistoryLength]`: floats, each sample's mean individual spawn time (timesteps)

### Summaries

- Summary files contain the following dimensions:
    - `n`: Indicates individual
    - `monitoringPoints` (designated below as `p`): number of monitoring points
    - `historyLength` (designated as `t` below): population history length, equivalent to number of timesteps
- Summary files contain the following variables:
    - `recruitTime[n]`: ints, each fish's entry timestep
    - `exitTime[n]`: ints, each fish's exit or death timestep (only valid for a given `n` if `finalStatus[n]` is not 0, since status 0 is Alive)
    - `entryForkLength[n]`: floats, fork length of each fish upon entry (mm)
    - `entryMass[n]`: floats, mass of each fish upon entry (g)
    - `finalForkLength[n]`: floats, current/final fork length of each fish (mm)
    - `finalMass[n]`: floats, current/final mass of each fish (g)
    - `finalStatus[n]`: ints, current/final status of each fish
- Summary files also contain the following monitoring point metadata:
    - `monitoringPointIDs[p]`: int, external id of each monitoring point
    - `monitoringPopulation[p][t]`: int, population at each monitoring point by timestep
    - `monitoringPopulationDensity[p][t]`: float, population density at each monitoring point by timestep
    - `monitoringDepth[p][t]`: float, depth at each monitoring point by timestep
    - `monitoringTemp[p][t]`: float, temperature at each monitoring point by timestep
  
### Tagged Fish Histories

- Tagged fish histories contain the following dimensions:
    - `n`: Indicates individual
    - `t`: Indicates timestep
- Tagged fish histories contain the following variables:
    - `recruitTime[n]`: ints, each fish's entry timestep
    - `taggedTime[n]`: ints, the timestep when each fish was tagged
    - `exitTime[n]`: ints, each fish's exit or death timestep (only valid for a given `n` if `finalStatus[n]` is not 0, since status 0 is Alive)
    - `entryForkLength[n]`: floats, fork length of each fish upon entry (mm)
    - `entryMass[n]`: floats, mass of each fish upon entry (g)
    - `finalForkLength[n]`: floats, current/final fork length of each fish (mm)
    - `finalMass[n]`: floats, current/final mass of each fish (g)
    - `finalStatus[n]`: ints, current/final status of each fish
    - `locationHistory[n, t]`: ints, the map node ID where each fish was on each timestep (or -1 if a fish wasn't in the model on the timestep in question)
    - `growthHistory[n, t]`: floats, the growth (g) on each timestep for each fish, or 0 if the fish wasn't in the model on the timestep in question
    - `pmaxHistory[n, t]`: floats, the pmax (p) on each timestep for each fish, or 0 if the fish wasn't in the model on the timestep in question
    - `mortalityHistory[n, t]`: floats, the mortality risk (probability) on each timestep for each fish, or 0 if the fish wasn't in the model on the timestep in question
    - `tempHistory[n, t]`: floats, the temperature in the current location at the end of each timestep for each fish, or 0 if the fish wasn't in the model on the timestep in question
    - `depthHistory[n, t]`: floats, the water depth in the location at the end of each timestep for each fish, or 0 if the fish wasn't in the model on the timestep in question
    - `flowSpeedHistory[n, t]`: floats, the flow speed in the location at the end of each timestep for each fish, or 0 if the fish wasn't in the model on the timestep in question
    - `flowVelocityUHistory[n, t]`: floats, the flow velocity u component in the location at the end of each timestep for each fish, or 0 if the fish wasn't in the model on the timestep in question
    - `flowVelocityVHistory[n, t]`: floats, the flow velocity v component in the location at the end of each timestep for each fish, or 0 if the fish wasn't in the model on the timestep in question
  
### Metadata

- `id_mapping_{#}.nc` : contains two parallel arrays, `externalNodeIds` and `internalNodeIds`
  that represent the mappings between external file-based node ids (driven by "vertices.csv") and internal node ids.
  This file is useful in external post-processing, for converting back from internal node ids in the model output
  files to external ids for comparison with the content of original input data files.
- `hydro_mapping_{#}.csv` : csv file describing the mapping of map nodes to hydro nodes. It contains three columns: internal node ID, hydro node ID, distance. 
  "Distance" is the distance along edges to the nearest hydro node.