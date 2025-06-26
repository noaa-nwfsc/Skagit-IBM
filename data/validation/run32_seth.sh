#!/bin/bash
echo "Starting run 32..."
starttime="$(date +"%T")"
echo "Current time : $starttime"

# Specify the folder you want to check/create
folder_path="run32"

# Check if the folder exists
if [ ! -d "$folder_path" ]; then
  # Create the folder if it doesn't exist
  mkdir -p "$folder_path"
  echo "Folder created: $folder_path"
else
  echo "Folder already exists: $folder_path"
fi

bin/headless "test_run_listings_reepicheep.csv" "run32/map2000d2006" "run29_config/config_map_2000_data_2006.json"
now="$(date +"%T")"
echo "completed config 1 of 9 at $now"

bin/headless "test_run_listings_reepicheep.csv" "run32/map2000d2010" "run29_config/config_map_2000_data_2010.json"
now="$(date +"%T")"
echo "completed config 2 of 9 at $now"

bin/headless "test_run_listings_reepicheep.csv" "run32/map2000d2014" "run29_config/config_map_2000_data_2014.json"
now="$(date +"%T")"
echo "completed config 3 of 9 at $now"

bin/headless "test_run_listings_reepicheep.csv" "run32/map2004d2006" "run29_config/config_map_2004_data_2006.json"
now="$(date +"%T")"
echo "completed config 4 of 9 at $now"

bin/headless "test_run_listings_reepicheep.csv" "run32/map2004d2010" "run29_config/config_map_2004_data_2010.json"
now="$(date +"%T")"
echo "completed config 5 of 9 at $now"

bin/headless "test_run_listings_reepicheep.csv" "run32/map2004d2014" "run29_config/config_map_2004_data_2014.json"
now="$(date +"%T")"
echo "completed config 6 of 9 at $now"

bin/headless "test_run_listings_reepicheep.csv" "run32/map2013d2006" "run29_config/config_map_2013_data_2006.json"
now="$(date +"%T")"
echo "completed config 7 of 9 at $now"

bin/headless "test_run_listings_reepicheep.csv" "run32/map2013d2010" "run29_config/config_map_2013_data_2010.json"
now="$(date +"%T")"
echo "completed config 8 of 9 at $now"

bin/headless "test_run_listings_reepicheep.csv" "run32/map2013d2014" "run29_config/config_map_2013_data_2014.json"
now="$(date +"%T")"
echo "completed config 9 of 9 at $now"

echo "Completed run 32"
echo "Started running at $starttime"
now="$(date +"%T")"
echo "Completed all configurations at $now"



[surion@nwcreepicheep Skagit-IBM]$ cat run29_config/config_map_2000_data_2014.json
{
    "envDataType": "file",
    "recStartTimestep": 0,
    "hydroStartTimestep": 0,
    "recruitEntryNodes": [29331,29346,29401],
    "recruitCountsFile": "data/skagit_migrants_2014_feb_start.csv",
    "recruitSizesFile": "data/skagit_weekly_lengths_2014_ed.csv",
    "samplingSitesFile": "data/sampling_sites.csv",
    "mapNodesFile": "data/maps_dec/Skagit2000_vertices_2019_elev.csv",
    "mapEdgesFile": "data/maps_dec/Skagit2000_edges_new.csv",
    "mapGeometryFile": "data/maps_dec/Skagit2000_geometry_new.csv",
    "tideFile": "data/cresTide.csv",
    "flowVolFile": "data/flowM3ps.csv",
    "airTempFile": "data/airTempC.csv",
    "flowSpeedFile": "data/mod_flow_condensed.nc",
    "distribWseTempFile": "data/wse.temp.full.nc",
    "threadCount": 96,
    "blindChannelSimplificationRadius": 0.0,
    "habitatTypeExitConditionHours": 48
}