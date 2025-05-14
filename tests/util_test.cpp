//
// Created by Troy Frever on 3/25/25.
//

#include <catch2/catch_test_macros.hpp>
#include <random>
#include "util.h"

#include "catch2/matchers/catch_matchers.hpp"


TEST_CASE("integer rand") {
    int offset = 3;
    int min = 3;
    int max = 5;

    SECTION("returns an int between requested min and max") {
        int counters[] = {0, 0, 0};

        for (int i = 0; i < 1000; ++i) {
            int result = GlobalRand::int_rand(min, max);

            REQUIRE(result >= min);
            REQUIRE(result <= max);

            counters[result - offset]++;
        }
        REQUIRE(counters[0] > 100);
        REQUIRE(counters[1] > 100);
        REQUIRE(counters[2] > 100);
    }

    SECTION("reseed produces same int_rand sequence") {
        int seed = 42;
        int num_rands = 10;
        std::vector<int> first_results;
        std::vector<int> second_results;
        first_results.reserve(num_rands);
        second_results.reserve(num_rands);

        GlobalRand::reseed(seed);
        for (int i = 0; i < num_rands; ++i) {
            first_results.push_back(GlobalRand::int_rand(min, max));
        }

        GlobalRand::reseed(seed);
        for (int i = 0; i < num_rands; ++i) {
            second_results.push_back(GlobalRand::int_rand(min, max));
        }

        bool same = true;
        for (int i = 0; i < num_rands; ++i) {
            if (first_results[i] != second_results[i]) {
                same = false;
            }
        }
        REQUIRE(same == true);
    }
}

TEST_CASE("normal rand gives two different consecutive values") {
    REQUIRE(GlobalRand::unit_normal_rand() != GlobalRand::unit_normal_rand());
}

TEST_CASE("unit rand gives two different consecutive values") {
    REQUIRE(GlobalRand::unit_rand() != GlobalRand::unit_rand());
}

TEST_CASE("randomness reseeding", "[rand]") {
    std::vector<float> first_results;
    std::vector<float> second_results;
    int num_rands = 5;

    SECTION("reseed_random gives a new random sequence") {
        GlobalRand::reseed_random();
        for (int i = 0; i < num_rands; ++i) {
            first_results.push_back(GlobalRand::unit_rand());
        }

        GlobalRand::reseed_random();
        for (int i = 0; i < num_rands; ++i) {
            second_results.push_back(GlobalRand::unit_rand());
        }

        bool same = true;
        for (int i = 0; i < num_rands; ++i) {
            if (first_results[i] != second_results[i]) {
                same = false;
            }
        }
        REQUIRE(same == false);
    }

    SECTION("reseed(RANDOMIZED_RNG_SEED) gives a new random sequence") {
        GlobalRand::reseed(GlobalRand::USE_RANDOM_SEED);
        for (int i = 0; i < num_rands; ++i) {
            first_results.push_back(GlobalRand::unit_rand());
        }

        GlobalRand::reseed(GlobalRand::USE_RANDOM_SEED);
        for (int i = 0; i < num_rands; ++i) {
            second_results.push_back(GlobalRand::unit_rand());
        }

        bool same = true;
        for (int i = 0; i < num_rands; ++i) {
            if (first_results[i] != second_results[i]) {
                same = false;
            }
        }
        REQUIRE(same == false);
    }

    SECTION("reseed twice with same seed gives same sequence") {
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
        REQUIRE(same == true);
    }
    SECTION("can return to earlier seed sequence") {
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
        REQUIRE(same == true);
    }
}
