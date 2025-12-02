cmake --build ./build/debug-hack --target headless --verbose; cp troy_run_listings_template.csv \
  troy_run_listings.csv; date; ./bin/Debug/headless "troy_run_listings.csv" "run32/map2000d2014" \
  "run29_config/config_map_2000_data_2014_deterministic.json"; date