//
// Created by Troy Frever on 4/21/25.
//

#include <catch2/catch_test_macros.hpp>
#include "load_utils.h"

SCENARIO("Correct missing netcdf values", "[load]") {
    GIVEN("a reference to one cell from a loaded netcdf vector") {
        const float MISSING_INDICATOR = 99;
        const float LAST_GOOD = 1000;
        float last_good = LAST_GOOD;
        const float DEFAULT_CELL = 42;
        float cell = DEFAULT_CELL;

        WHEN("that cell contains a normal value") {
            fix_missing_value(cell, last_good, MISSING_INDICATOR);
            THEN("the cell is unchanged") {
                REQUIRE(cell == DEFAULT_CELL);
                AND_THEN("the last good value is updated") {
                    REQUIRE(last_good == DEFAULT_CELL);
                }
            }
        }
        WHEN("that cell contains the missing indicator fill value") {
            cell = MISSING_INDICATOR;
            fix_missing_value(cell, last_good, MISSING_INDICATOR);
            THEN("the cell is updated with the replacement value") {
                REQUIRE(cell == LAST_GOOD);
                AND_THEN("the last good value is unchanged") {
                    REQUIRE(last_good == LAST_GOOD);
                }
            }
        }
        WHEN("that cell contains the missing indicator FILL_VALUE") {
        }
    }
    GIVEN("the fill value is NaN") {
        const float NaN_value = NAN;
        WHEN("the cell contains NaN") {
            float last_good = 1000;
            float cell = NaN_value;
            fix_missing_value(cell, last_good, NaN_value);
            THEN("the cell is updated with the replacement value") {
                REQUIRE(cell == last_good);
            }
        }
    }
}

SCENARIO("find first good value") {
    GIVEN("a float vector and a missing indicator") {
        const float MISSING_INDICATOR = std::numeric_limits<float>::max();
        std::vector<float> values = {1, 2, 3};
        WHEN("the first element is not the missing indicator") {
            float result = find_first_non_missing_value(values, MISSING_INDICATOR);
            THEN("the value of the first element is found")
                REQUIRE(result == 1);
        }
        WHEN("the first few elements contain the missing indicator") {
            values = {MISSING_INDICATOR, MISSING_INDICATOR, 3, 4};
            float result = find_first_non_missing_value(values, MISSING_INDICATOR);
            THEN("the value of the first non-missing element is found")
                REQUIRE(result == 3);
        }
        WHEN("all the values are the missing indicator") {
            values = {MISSING_INDICATOR, MISSING_INDICATOR, MISSING_INDICATOR};
            THEN("the missing indicator is returned") {
                REQUIRE(find_first_non_missing_value(values, MISSING_INDICATOR) == MISSING_INDICATOR);
            }
        }
    }
    GIVEN("a float vector with NaN as the missing indicator") {
        float NaN_indicator = NAN;
        WHEN("the first few elements are missing") {
            const std::vector<float> values = {NaN_indicator, NaN_indicator, 42};
            THEN("the first non-missing element is found") {
                REQUIRE(find_first_non_missing_value(values, NaN_indicator) == 42);
            }
        }
    }
}
