//
// Created by Troy Frever on 6/9/25.
//

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <limits>
#include <cmath>

#include "load_utils.h"

TEST_CASE("is_missing_indicator tests", "[missing_values]") {
    float epsilon = std::numeric_limits<float>::epsilon();

    SECTION("Regular value comparisons") {
        REQUIRE(is_missing_indicator(42.0f, 42.0f) == true);
        REQUIRE(is_missing_indicator(42.0f, 43.0f) == false);
        REQUIRE(is_missing_indicator(0.0f, 0.0f) == true);
    }

    SECTION("NaN handling") {
        float nan = std::numeric_limits<float>::quiet_NaN();
        REQUIRE(is_missing_indicator(nan, nan) == true);
        REQUIRE(is_missing_indicator(42.0f, nan) == false);
        REQUIRE(is_missing_indicator(nan, 42.0f) == false);
    }

    SECTION("Infinity handling") {
        float pos_inf = std::numeric_limits<float>::infinity();
        float neg_inf = -std::numeric_limits<float>::infinity();

        REQUIRE(is_missing_indicator(pos_inf, pos_inf) == true);
        REQUIRE(is_missing_indicator(neg_inf, neg_inf) == true);
        REQUIRE(is_missing_indicator(pos_inf, neg_inf) == false);
        REQUIRE(is_missing_indicator(neg_inf, pos_inf) == false);
        REQUIRE(is_missing_indicator(42.0f, pos_inf) == false);
        REQUIRE(is_missing_indicator(pos_inf, 42.0f) == false);
    }

    SECTION("Small number comparisons") {
        float tiny = epsilon * 10.0f;

        REQUIRE(is_missing_indicator(tiny, tiny) == true);
        REQUIRE(is_missing_indicator(tiny, tiny * 1.1f) == false);
        REQUIRE(is_missing_indicator(0.0f, tiny) == false);
    }

    SECTION("Large number comparisons") {
        float large = std::numeric_limits<float>::max() / 2.0f;

        REQUIRE(is_missing_indicator(large, large) == true);
        REQUIRE(is_missing_indicator(large, large * 1.1f) == false);
    }

    SECTION("Near-zero comparisons") {
        float near_zero = std::numeric_limits<float>::min();

        REQUIRE(is_missing_indicator(near_zero, near_zero) == true);
        REQUIRE(is_missing_indicator(-near_zero, near_zero) == false);
        REQUIRE(is_missing_indicator(0.0f, near_zero) == false);
    }

    SECTION("Edge cases") {
        float zero = 0.0f;
        float neg_zero = -0.0f;

        REQUIRE(is_missing_indicator(zero, neg_zero) == true);
        REQUIRE(is_missing_indicator(zero, zero) == true);
        REQUIRE(is_missing_indicator(neg_zero, neg_zero) == true);
    }


    SECTION("Nearly equal values within epsilon") {
        const float base = 1.0f;

        REQUIRE(is_missing_indicator(base, base + (base * epsilon * 0.5f)) == true);
        REQUIRE(is_missing_indicator(base, base - (base * epsilon * 0.5f)) == true);

        REQUIRE(is_missing_indicator(base, base + (base * epsilon * 2.0f)) == false);
        REQUIRE(is_missing_indicator(base, base - (base * epsilon * 2.0f)) == false);
    }

    SECTION("Nearly equal larger values within epsilon") {
        const float base = 1000.0f;

        REQUIRE(is_missing_indicator(base, base + (base * epsilon * 0.5f)) == true);
        REQUIRE(is_missing_indicator(base, base - (base * epsilon * 0.5f)) == true);

        REQUIRE(is_missing_indicator(base, base + (base * epsilon * 2.0f)) == false);
        REQUIRE(is_missing_indicator(base, base - (base * epsilon * 2.0f)) == false);
    }
}
