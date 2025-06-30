#ifndef __FISH_LOAD_H
#define __FISH_LOAD_H

#include <vector>
#include <string>
#include <unordered_map>

#include "map.h"

// Utility function to split a string into chunks delimited by a given character
std::vector<std::string> split(std::string& s, char c);

// Loads distributary hydrology data from two NetCDF3/4 files into a vector of DistribHydroNodes (defined in map.h)
// See CONFIG_README for a description of the file formats
void loadDistribHydro(std::string &flowPath, std::string &wseTempPath, std::vector<DistribHydroNode> &nodesOut);

// Loads recruit size distributions from a CSV file into a 2d float vector
// See CONFIG_README for a description of the file format
void loadRecSizeDists(std::string &filePath, std::vector<std::vector<float>> &out);

// Loads a list of integers from a CSV file into a vector of ints
void loadIntList(std::string &filePath, std::vector<int> &out);
std::vector<int> loadIntList(std::string &filePath);

// Loads a list of floats from a CSV file into a vector of floats
void loadFloatList(std::string &filePath, std::vector<float> &out);
std::vector<float> loadFloatList(std::string &filePath);

// Loads a list of floats from a CSV file, omitting all but every Nth value
std::vector<float> loadFloatListInterleaved(std::string &filePath, int n);

void checkAndAddEdge(Edge e);

// Loads a list of sampling sites from a CSV file into a vector of SamplingSites (defined in map.h)
//void loadSamplingSites(std::string &filePath, std::vector<MapNode *> &map, std::vector<SamplingSite> &out);

// Loads the map from a CSV location file, a CSV edge file, and a CSV geometry file
// The resulting heap-allocatd MapNodes are placed in the vector 'dest'
// See CONFIG_README for a description of file formats
void loadMap(
    std::vector<MapNode *> &dest,
    std::string locationFilePath,
    std::string edgeFilePath,
    std::string geometryFilePath,
    std::unordered_map<unsigned int, unsigned int> &csvToInternalID,
    std::vector<DistribHydroNode> &hydroNodes,
    std::vector<unsigned> &recPointIds,
    std::vector<MapNode *> &recPoints,
    std::vector<MapNode *> &monitoringPoints,
    std::vector<SamplingSite *> &samplingSites,
    float blindChannelSimplificationRadius);

#endif