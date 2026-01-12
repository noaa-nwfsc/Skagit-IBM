# Release Notes

This file records major feature changes. It goes back in time to 11.19.2024. "Releases" are actually just dates
when the completed feature was merged to the main branch. Functional parts of a feature may have been merged earlier.
Minor updates are not recorded.

## 01.12.2026
- new configurable float input parameters for `growthSlopeNearshore`, `pmaxUpperLimit`, `pmaxUpperLimitNearshore`, and `pmaxLowerLimit`

## 01.01.2026
- "high" now available as a value for `agentAwareness`.

## 11.17.2025
- indexing into the `recruitSizesFile` is corrected to be weekly instead of bi-weekly. This includes changes to how 
  the indexing into hydro data is done.

## 11.03.2025
- `mortalityInflectionPoint` is now configurable in the config json file.

## 09.22.2025
- `agentAwareness` model of fish movement implemented. Values of `low` and `medium` are now supported. Directionless 
  edges are now required.

## 09.15.2025
- the `id` field of a node (found in some output files) matches the external node id found in csv input files.

## 07.28.2025
- `mortMin`, `mortMax`, growthSlope`, and `virtualNodes` are now configurable in the config json file.

## 07.28.2025
- use the `habitatMortalityMultiplier` in DistributaryEdge nodes in addition to the previous Distributary and Nearshore nodes

## 06.30.2025
- duplicate edges are no longer added

## 06.16.2025
- blind channel flow scaling now functioning

## 05.30.2025
- directionless edges are now supported

## 04.29.2025
- catch various input file parsing errors and recover if possible, else abort

## 04.07.2025
- `rng_seed` is now available as a parameter in the config json for specifying a fixed seed to the random number
  generator. See [CONFIG_README.md](CONFIG_README.md) for further details.
- new automated unit test framework.

## 03.03.2025
- nearest hydro node is now calculated using distance along edges rather than as the crow flies, using a Dijkstra algorithm.
- new output file `hydro_mapping_{#}.csv` : csv file describing the mapping of map nodes to hydro nodes. It contains three columns: internal node ID, hydro node ID, distance.
  "Distance" is the distance along edges to the nearest hydro node.
- blind channel consolidation now works properly, including related bug fixes in input datafile parsing.
- improved water velocity implementation.
- new flow/width equation for blind channels.

## 02.11.2025
- added configuration to .json input file for habitat-based mortality multiplier:
  - `habitatMortalityMultiplier`: float; default 2.0; additional mortality multiplier applied in distributaries and 
    nearshore habitats.
    See [default_config_env_from_file.json](default_config_env_from_file.json) for an example usage.

## 01.13.2025

- `tempHistoryOut` is included in the tagged history output files.
- `depthHistoryOut` is included in the tagged history output files.
- `flowSpeedHistoryOut` is included in the tagged history output files.

## 12.17.2024
- added configuration to .json input file to control the length of nearshore residences before exiting. Specifically:
  - `habitatTypeExitConditionHours`: float; optional, default 2.0; the number of consecutive hours a fish must reside 
    in a Nearshore habitat (at the end of each hour) after which it will "exit" the simulation. 
    See [default_config_env_from_file.json](default_config_env_from_file.json) for an example usage.

## 11.26.2024

- `pmaxHistory` is included in the tagged history output files.
- `id_mapping_{n}.nc` is a new output file. It contains two parallel arrays, `externalNodeIds` and `internalNodeIds`
  that represent the mappings between external file-based node ids (driven by "vertices.csv") and internal node ids.
  This file is useful in external post-processing, for converting back from internal node ids in the model output
  files to external ids for comparison with the content of original input data files.

## 11.19.2024

### Updated temperature and elevation models

**Temperature**

- The base water temperature in any node, regardless of node type, is set as equivalent to the temperature in the
  current timestep at the nearest Hydro Node.
- The base temperature is then limited to a range as follows:
    - The maximum water temperature is 30° C.
    - The minimum water temperature in distributaries and harbors is 4° C; the minimum water temperature in all other
      nodes is 0.01° C (this was the previously used value so I kept it).

**Pre-existing temperature usage, still in effect, unchanged:**

- In fish.cpp there is a max consumption temp (CTM) of 25° C (with comment saying it was 18). This affects the
  consumption/growth calculations, but does not change the base water temperature.

**Depth**

- The base depth in any node, regardless of node type, is set as equivalent to the surface elevation in the current
  timestep at the nearest Hydro Node, minus the node elevation for the node in question.
- The base depth is then limited to a range as follows:
    - The minimum depth for distributaries and harbors is 0.2 meters. The minimum depth in all other nodes is 0.
    - There is no enforced maximum depth.

**Pre-existing depth usage, still in effect, unchanged:**

- Depth is used in fish.cpp when finding nodes in swim range. Sources with depths below 0.2 will not move fish.
  If depth is <= 0.0, fish will "die stranded".
