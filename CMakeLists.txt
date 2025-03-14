cmake_minimum_required(VERSION 3.10)
project(headless_gui VERSION 1.0)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")

# C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler options
add_compile_options(-Wall -Wextra)

# Build type specific options
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-g -O0 -fno-inline -fstack-protector-all)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_compile_options(-O2 -ffast-math -DNDEBUG -march=native)
endif()

# Find packages
find_package(wxWidgets COMPONENTS core base QUIET)

# Include directories
include_directories(
  ${CMAKE_SOURCE_DIR}/rapidjson/include
  ${CMAKE_SOURCE_DIR}/local/netcdf-cxx4/include
  ${CMAKE_SOURCE_DIR}/local/netcdf-c/include
  ${CMAKE_SOURCE_DIR}/local/hdf5/include
)

# Set link directories
link_directories(
  ${CMAKE_SOURCE_DIR}/local/netcdf-cxx4/lib
  ${CMAKE_SOURCE_DIR}/local/netcdf-c/lib
)

# Define source files used for both targets
set(COMMON_SOURCES
  src/model.cpp
  src/hydro.cpp
  src/load.cpp
  src/fish.cpp
  src/map.cpp
  src/util.cpp
  src/env_sim.cpp
  src/map_gen.cpp
)

# Get absolute paths for rpath
get_filename_component(ABSLIB_NCCPP "${CMAKE_SOURCE_DIR}/local/netcdf-cxx4/lib" ABSOLUTE)
get_filename_component(ABSLIB_NCC "${CMAKE_SOURCE_DIR}/local/netcdf-c/lib" ABSOLUTE)

# Determine dynamic library extension
if(APPLE)
  set(DL_EXT "dylib")
else()
  set(DL_EXT "so")
endif()

# Create headless executable
add_executable(headless ${COMMON_SOURCES} src/headless.cpp)
set_target_properties(headless PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
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
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
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
