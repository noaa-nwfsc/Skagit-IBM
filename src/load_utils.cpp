//
// Created by Troy Frever on 4/21/25.
//
#include <cmath>
#include <vector>

bool is_missing_indicator(const float value, const float missing_indicator) {
    if (std::isnan(missing_indicator))
        return std::isnan(value);
    return value == missing_indicator;
}

bool fix_missing_value(float &cell, float &last_good_value, const float missing_indicator) {
    if (is_missing_indicator(cell, missing_indicator)) {
        cell = last_good_value;
        return true;
    }
    last_good_value = cell;
    return false;
}

float find_first_non_missing_value(const std::vector<float> &values, const float missing_indicator) {
    for (float value: values) {
        if (!is_missing_indicator(value, missing_indicator)) {
            return value;
        }
    }
    return missing_indicator;
}
