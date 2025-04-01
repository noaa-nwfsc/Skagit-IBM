#include <catch2/catch_test_macros.hpp>
#include "hydro.h"

TEST_CASE( "hydro updates timestep", "" ) {
  std::vector<MapNode *> map;
  std::vector<std::vector<float>> depths;
  std::vector<std::vector<float>> temps;
  float distFlow = 0;

  HydroModel hydroModel = HydroModel(map, depths, temps, distFlow);
  hydroModel.updateTime(4);
  REQUIRE( hydroModel.getTime() == 4 );
  hydroModel.updateTime(7);
  REQUIRE( hydroModel.getTime() == 7 );
}
