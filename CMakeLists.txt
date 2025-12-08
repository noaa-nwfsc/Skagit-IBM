# examples
# configure: "cmake -DCMAKE_BUILD_TYPE=Debug -S ~/code/Skagit-IBM -B ~/code/Skagit-IBM/build/debug"
# build    : "cmake --build ~/code/Skagit-IBM/build/debug"
# build a target: "cmake --build ~/code/Skagit-IBM/build/debug --target headless --verbose"
# clean    : "cmake --build ~/code/Skagit-IBM/build/debug --target clean"
#
# configure release build with ASAN and debugging info:
#    "cmake -DCMAKE_BUILD_TYPE=RelWithDebugStack -DENABLE_ASAN=ON -S ~/code/Skagit-IBM -B ~/code/Skagit-IBM/build/releaseWithSyms"
# and build it: "cmake --build ~/code/Skagit-IBM/build/releaseWithSyms --target headless"
# a good set of ASAN options to run that build with (note the suppression of leak detection):
# "export ASAN_OPTIONS=detect_odr_violation=0:alloc_dealloc_mismatch=0:halt_on_error=0:print_stacktrace=1:quarantine_size_mb=256:verbosity=0:sleep_before_dying=30:detect_leaks=0"
#
# configure debug build with QUICK_DEBUG_HACK defined. note: this puts the build in a different directory, "debug-hack"
#    "cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS} -DQUICK_DEBUG_HACK" -S ~/code/Skagit-IBM -B ~/code/Skagit-IBM/build/debug-hack"
#
# executables show up in {Skagit-IBM-ROOT}/bin under a subdirectory (e.g. /Debug or /Release)

cmake_minimum_required(VERSION 3.10)
project(headless_gui VERSION 1.0)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Compiler options
add_compile_options(-Wall -Wextra)

# Build type specific options
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-g -O0 -fno-inline -fstack-protector-all -ffp-contract=off)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_compile_options(-O2 -ffast-math -fno-finite-math-only -DNDEBUG -march=native)
endif()

# Add a new build type: RelWithDebugStack with extra diagnostics
set(CMAKE_CXX_FLAGS_RELWITHDEBUGSTACK "-O2 -ffast-math -fno-finite-math-only -DNDEBUG -march=native -g -fno-omit-frame-pointer")
set(CMAKE_C_FLAGS_RELWITHDEBUGSTACK "${CMAKE_CXX_FLAGS_RELWITHDEBUGSTACK}")
set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBUGSTACK "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBUGSTACK "${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")

set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES};RelWithDebugStack" CACHE STRING "" FORCE)
if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebugStack")
  message(STATUS "Build type: RelWithDebugStack - Release with debug symbols and frame pointers")
endif()

# Define sanitizer option (OFF by default)
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)

# Apply sanitizer settings if enabled
if(ENABLE_ASAN)
  message(STATUS "Address Sanitizer enabled")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
endif()

# Find packages
find_package(wxWidgets COMPONENTS core base QUIET)

# Include directories
include_directories(
  ${CMAKE_SOURCE_DIR}/rapidjson/include
  ${CMAKE_SOURCE_DIR}/local/netcdf-cxx4/include
  ${CMAKE_SOURCE_DIR}/local/netcdf-c/include
  ${CMAKE_SOURCE_DIR}/local/hdf5/include
  ${CMAKE_SOURCE_DIR}/src
)

add_subdirectory(3rdParty/Catch2)
add_subdirectory(tests)

# Set link directories
link_directories(
  ${CMAKE_SOURCE_DIR}/local/netcdf-cxx4/lib
  ${CMAKE_SOURCE_DIR}/local/netcdf-c/lib
)

# Define source files used for both targets
set(COMMON_SOURCES
  src/env_sim.cpp
  src/fish.cpp
  src/fish_movement.cpp
  src/fish_movement_downstream.cpp
  src/fish_movement_factory.cpp
  src/hydro.cpp
  src/load.cpp
  src/load_utils.cpp
  src/map.cpp
  src/map_gen.cpp
  src/model.cpp
  src/model_config_map.cpp
  src/util.cpp
  src/fish_movement_high_awareness.cpp
)

# Get absolute paths for rpath
get_filename_component(ABSLIB_NCCPP "${CMAKE_SOURCE_DIR}/local/netcdf-cxx4/lib" ABSOLUTE)
get_filename_component(ABSLIB_NCC "${CMAKE_SOURCE_DIR}/local/netcdf-c/lib" ABSOLUTE)

# Determine dynamic library extension
if(NOT DL_EXT)
  if(APPLE)
    set(DL_EXT "dylib" CACHE STRING "Dynamic library extension")
  else()
    set(DL_EXT "so" CACHE STRING "Dynamic library extension")
  endif()
endif()
# Create headless executable
add_executable(headless ${COMMON_SOURCES} src/headless.cpp)
set_target_properties(headless PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}
)

# NetCDF and other libraries for headless
target_link_libraries(headless
  ${CMAKE_SOURCE_DIR}/local/netcdf-cxx4/lib/libnetcdf_c++4.${DL_EXT}
  ${CMAKE_SOURCE_DIR}/local/netcdf-c/lib/libnetcdf.${DL_EXT}
  pthread
)

# Set rpath
if(APPLE)
  set_target_properties(headless PROPERTIES
    INSTALL_RPATH "${ABSLIB_NCCPP};${ABSLIB_NCC}"
    BUILD_WITH_INSTALL_RPATH TRUE
  )
else()
  set_target_properties(headless PROPERTIES
    INSTALL_RPATH "${ABSLIB_NCCPP};${ABSLIB_NCC}"
    BUILD_WITH_INSTALL_RPATH TRUE
    LINK_FLAGS "-Wl,-rpath,${ABSLIB_NCCPP} -Wl,-rpath,${ABSLIB_NCC}"
  )
endif()

# Create GUI executable if wxWidgets is available
if(wxWidgets_FOUND)
  include(${wxWidgets_USE_FILE})
  
  add_executable(gui ${COMMON_SOURCES} src/gui.cpp)
  set_target_properties(gui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}
  )
  
  target_compile_definitions(gui PRIVATE ${wxWidgets_DEFINITIONS})
  target_include_directories(gui PRIVATE ${wxWidgets_INCLUDE_DIRS})
  
  target_link_libraries(gui
    ${wxWidgets_LIBRARIES}
    ${CMAKE_SOURCE_DIR}/local/netcdf-cxx4/lib/libnetcdf_c++4.${DL_EXT}
    ${CMAKE_SOURCE_DIR}/local/netcdf-c/lib/libnetcdf.${DL_EXT}
    pthread
  )
  
  # Set rpath for GUI too
  if(APPLE)
    set_target_properties(gui PROPERTIES
      INSTALL_RPATH "${ABSLIB_NCCPP};${ABSLIB_NCC}"
      BUILD_WITH_INSTALL_RPATH TRUE
    )
  else()
    set_target_properties(gui PROPERTIES
      INSTALL_RPATH "${ABSLIB_NCCPP};${ABSLIB_NCC}"
      BUILD_WITH_INSTALL_RPATH TRUE
      LINK_FLAGS "-Wl,-rpath,${ABSLIB_NCCPP} -Wl,-rpath,${ABSLIB_NCC}"
    )
  endif()
  
  # Add alias target
  add_custom_target(build_gui DEPENDS gui)
else()
  message(STATUS "wxWidgets not found - GUI target will not be available")
endif()

# Clean target similar to original Makefile
add_custom_target(clean_all
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_SOURCE_DIR}/bin
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
  COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/CMakeCache.txt
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/cmake_install.cmake
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/Makefile
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/bin
)

# Create build directories
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
