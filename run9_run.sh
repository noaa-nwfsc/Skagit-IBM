#!/bin/bash
echo "Starting run 9..."
now="$(date +"%T")"
echo "Current time : $now"
bin/headless "test_run_listings_reepicheep.csv" "run9/map2013d2014" "run6_config/config_map_2013_data_2014.json"
now="$(date +"%T")"
echo "completed config 1 at $now"

bin/headless "test_run_listings_reepicheep.csv" "run9/map2013d2010" "run6_config/config_map_2013_data_2010.json"
now="$(date +"%T")"
echo "completed config 2 at $now"

bin/headless "test_run_listings_reepicheep.csv" "run9/map2013d2006" "run6_config/config_map_2013_data_2006.json"
now="$(date +"%T")"
echo "completed config 3 at $now"

bin/headless "test_run_listings_reepicheep.csv" "run9/map2004d2014" "run6_config/config_map_2004_data_2014.json"
now="$(date +"%T")"
echo "completed config 4 at $now"

bin/headless "test_run_listings_reepicheep.csv" "run9/map2004d2010" "run6_config/config_map_2004_data_2010.json"
now="$(date +"%T")"
echo "completed config 5 at $now"

bin/headless "test_run_listings_reepicheep.csv" "run9/map2004d2006" "run6_config/config_map_2004_data_2006.json"
now="$(date +"%T")"
echo "completed config 6 at $now"

bin/headless "test_run_listings_reepicheep.csv" "run9/map2000d2014" "run6_config/config_map_2000_data_2014.json"
now="$(date +"%T")"
echo "completed config 7 at $now"

bin/headless "test_run_listings_reepicheep.csv" "run9/map2000d2010" "run6_config/config_map_2000_data_2010.json"
now="$(date +"%T")"
echo "completed config 8 at $now"

bin/headless "test_run_listings_reepicheep.csv" "run9/map2000d2006" "run6_config/config_map_2000_data_2006.json"
now="$(date +"%T")"
echo "completed config 9 at $now"

now="$(date +"%T")"
echo "Completed allrun 7 configurations at $now"
