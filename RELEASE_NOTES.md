# Release Notes

This file records major feature changes. It goes back in time as far as 11.19.2024. "Releases" are actually just dates
when the completed feature was merged to the main branch. Functional parts of a feature may have been merged earlier.
Minor updates are not recorded.

## 12.17.2024
- added configuration to .json input file to control the length of nearshore residences before exiting. Specifically:
  - `habitatTypeExitConditionHours`: float; optional, default 2.0; the number of consecutive hours a fish must reside 
    in a Nearshore habitat (at the end of each hour) after which it will "exit" the simulation. 
    See [default_config_env_from_file.json](default_config_env_from_file.json) for an example usage.

## 11.26.2024

- `pmaxHistory` is included in the tagged history output files.
- `id_mapping_{n}.nc` is a new output file. It contains two parallel arrays, `externalNodeIds` and `internalNodeIds`
  that represent the mappings between external file-based node ids (driven by "vertices.csv") and internal node ids.

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
