
# Configuration

Default model configurations are in the files [default_config_env_sim.json](default_config_env_sim.json) and [default_config_env_from_file.json](default_config_env_from_file.json).

Parameters:
- `threadCount`: The maximum number of hardware threads to use when running the model (-1 = as many as are available)
- `rng_seed` (optional): Random number generator seed. If a positive non-zero value is specified, the model will use 
  the value to seed the random number generator. It will also limit the number of threads to 1 (overriding the 
  `threadCount` parameter. Together this will create reproducible, deterministic outputs for testing or validation. If
  `rng_seed` is 0 or not present, the random number generator will obtain a random seed, resulting in non-deterministic
  outputs across multiple runs.
- `envDataType`: string, either `file` or `sim`
    - if `envDataType` is `file`, the following entries are expected:
        - `recStartTimestep`: the number of 1-hour timesteps from midnight on January 1 to the start date/time of the recruitment data
        - `hydroStartTimestep`: the number of 1-hour timesteps from midnight on January 1 to the start date/time of the hydrology input data (tide, flow volume, and air temperature)
            
            Note: the flow speed data is assumed to start at midnight on January 1 regardless of `hydroStartTimestep`
        
        - `recruitEntryNodes`: an array of integer map node IDs at which new recruits will originate
        - `recruitCountsFile`: name of a text file containing one decimal number per line, each representing the number of recruits on a single day
        - `recruitSizesFile`: name of a CSV file where each row after the first represents the distribution of recruits among 5mm size-classes for a single week
        - `samplingSitesFile`: name of a CSV file where each row after the first describes a sampling site, giving its location and human-readable name
        - `mapNodesFile`: name of a CSV file where each row after the first describes a map point, with relevant fields arranged as follows:
            
            ID, _, _, _, _, area (m^2), habitat type, _, _, _, elev (m), _
            
            - Elev is the vertical distance to NAVD88 in meters.
            Habitat type is one of the following:
                `blind channel`,
                `impoundment`,
                `low tide terrace`,
                `distributary`,
                `boat harbor`,
                `nearshore`
            
            - ID is one-indexed (starts from 1) in map input files but the model converts this to a zero-based index.

        - `mapEdgesFile`: name of a CSV file where each row after the first describes a map edge, with relevant fields arranged as follows:

            _, _, _, _, _, _, _, _, _, _, _, _, _, _, source node ID, target node ID, _, _, length (m), _

            - The relevant fields are the 15th, 16th, and 19th fields.

        - `mapGeometryFile`: name of a CSV file where each row after the first gives the location of a certain node.

            Fields are: x, y, ID

            x and y are UTM Zone 10N coordinates.

        - `tideFile`: name of a text file where each line is a crescent tide height (m) measured every 15 minutes
        - `flowVolFile`: name of an ASCII file where each line is a flow volume (m^3/s), measured every 15 minutes upstream from the recruitment site
        - `airTempFile`: name of an ASCII file where each line is an air temperature, in degrees C, measured every 15 minutes
        - `flowSpeedFile`: name of a NetCDF-3 or NetCDF-4 file where the following variables are present:

            - `x`, `y`: the horizontal and vertical locations where flow velocity is given, in UTM Zone 10N coordinates

                dimensions: node (arbitrary number of locations)
            
            - `u`, `v`: the horizontal and vertical flow velocity (m/s)

                dimensions: time (1hr increments, starting at midnight on January 1st), node
        
        - `distribWseTempFile`: name of a NetCDF-3 or NetCDF-4 file where the following variables are present:

            - `x`, `y`: the horizontal and vertical locations where flow velocity is given, in UTM Zone 10N coordinates

                dimensions: node (arbitrary number of locations)
            
            - `wse`: the water surface elevation (in meters, from NAVD88 datum)

                dimensions: time (1hr increments, starting at midnight on January 1st), node

            - `temp`: the water temperature (in degrees C)

                dimensions: time (1hr increments, starting at midnight on January 1st), node

            Distributary nodes retrieve flow speed, depth, and temperature from their nearest hydro node, so these nodes should be relatively dense spatially in distributary regions.

        - `blindChannelSimplificationRadius`: float; the maximum distance between blind channel nodes that will result in them being merged when the map data is loaded (to speed up model prediction).
        - `habitatTypeExitConditionHours`: float; optional, default 2.0; the number of consecutive hours a fish must reside in a Nearshore habitat (at the end of each hour) after which it will "exit" the simulation.
        - `habitatMortalityMultiplier`: float; default 2.0; additional mortality multiplier applied in distributaries and nearshore habitats.
        - `directionlessEdges`: int; optional, default 0; treated as boolean determining whether or not to use directionless edges.
    - if `envDataType` is "sim", the following entries are expected:
        - `mapParams`: A subgroup of parameters containing the following keys:
            - `m`: int, the number of distributary nodes per grid row/column
            - `n`: int, the total number of nodes per grid row/column
            - `a`: float, the distance between grid nodes in meters
            - `pBlind`: float between 0 and 1, the probability of deleting a blind channel
            - `pDist`: float between 0 and 1, the probability of deleting a distributary channel
        - `recruitSizeMean`: float, the mean recruit size in millimeters
        - `recruitSizeStd`: float, the standard deviation of recruit sizes in millimeters
        - `recruitRate`: int, the average number of recruits per timestep
        - `simLength`: int, the number of timesteps of environmental data to generate