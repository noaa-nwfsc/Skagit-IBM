cmake_minimum_required(VERSION 3.16)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(MODEL_SOURCES
        ../src/hydro.cpp
        ../src/map.cpp
        ../src/load.cpp
        ../src/model_config_map.cpp
        ../src/load_utils.cpp
        ../src/util.cpp
        ../src/fish_movement.cpp
        ../src/fish_movement_downstream.cpp
        ../src/fish_movement_factory.cpp
        ../src/model.cpp
        ../src/map_gen.cpp
        ../src/fish.cpp
        ../src/env_sim.cpp
)

set(TEST_SOURCES
        fish_movement_test.cpp
        hydro_model_test.cpp
        load_utils_test.cpp
        util_test.cpp
        is_missing_indicator_test.cpp
        load_test.cpp
        fish_movement_pathfinding_test.cpp
        fish_movement_factory_test.cpp
        fish_move_test.cpp
)

# These tests can use the Catch2-provided main
add_executable(tests
        ${TEST_SOURCES}
        ${MODEL_SOURCES}
)

set_target_properties(tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}
)

target_link_libraries(tests PRIVATE
        ${CMAKE_SOURCE_DIR}/local/netcdf-cxx4/lib/libnetcdf_c++4.${DL_EXT}
        ${CMAKE_SOURCE_DIR}/local/netcdf-c/lib/libnetcdf.${DL_EXT}
        pthread
        Catch2::Catch2WithMain)


# These tests need their own main
#add_executable(custom-main-tests test.cpp test-main.cpp)
#target_link_libraries(custom-main-tests PRIVATE Catch2::Catch2)