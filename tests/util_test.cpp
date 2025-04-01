//
// Created by Troy Frever on 3/25/25.
//

#include <catch2/catch_test_macros.hpp>
#include <random>
#include "util.h"

#include "catch2/matchers/catch_matchers.hpp"


TEST_CASE( "normal rand gives two different consecutive values" ) {
    REQUIRE( GlobalRand::unit_normal_rand() != GlobalRand::unit_normal_rand() );
}

TEST_CASE( "unit rand gives two different consecutive values" ) {
    REQUIRE( GlobalRand::unit_rand() != GlobalRand::unit_rand() );
}

TEST_CASE("randomness reseeding", "[rand]") {
    std::vector<float> first_results;
    std::vector<float> second_results;
    int num_rands = 5;

    SECTION( "reseed without value gives a new random sequence" ) {
        for (int i = 0; i < num_rands; ++i) {
            first_results.push_back(GlobalRand::unit_rand());
        }

        GlobalRand::reseed();

        for (int i = 0; i < num_rands; ++i) {
            second_results.push_back(GlobalRand::unit_rand());
        }

        bool same = true;
        for (int i = 0; i < num_rands; ++i) {
            if (first_results[i] != second_results[i]) {
                same = false;
            }
        }
        REQUIRE( same == false );
    }

    SECTION( "reseed twice with same seed gives same sequence" ) {
        int seed = 3;

        GlobalRand::reseed(seed);
        for (int i = 0; i < num_rands; ++i) {
            first_results.push_back(GlobalRand::unit_rand());
        }

        GlobalRand::reseed(seed);

        for (int i = 0; i < num_rands; ++i) {
            second_results.push_back(GlobalRand::unit_rand());
        }

        bool same = true;
        for (int i = 0; i < num_rands; ++i) {
            if (first_results[i] != second_results[i]) {
                same = false;
            }
        }
        REQUIRE( same == true );
    }
    SECTION( "can return to earlier seed sequence" ) {
        int original_seed = 3;

        GlobalRand::reseed(original_seed);
        for (int i = 0; i < num_rands; ++i) {
            first_results.push_back(GlobalRand::unit_rand());
        }

        int different_seed = 5;
        GlobalRand::reseed(different_seed);
        GlobalRand::unit_rand();
        GlobalRand::unit_rand();
        GlobalRand::reseed(original_seed);

        for (int i = 0; i < num_rands; ++i) {
            second_results.push_back(GlobalRand::unit_rand());
        }

        bool same = true;
        for (int i = 0; i < num_rands; ++i) {
            if (first_results[i] != second_results[i]) {
                same = false;
            }
        }
        REQUIRE( same == true );
    }
}
