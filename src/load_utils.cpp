//
// Created by Troy Frever on 4/21/25.
//
#include <cmath>
#include <vector>
#include <netcdf>
#include "load_utils.h"


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

void fix_all_missing_values(size_t stepCount, const NcVarFillModeInterface &nc_var_vector, std::vector<float> &hydro_vector) {
    std::vector<std::string> log;
    bool is_fill_active;
    float missing_indicator;
    nc_var_vector.getFillModeParameters(is_fill_active, &missing_indicator);
    float nearby_good_value = find_first_non_missing_value(hydro_vector, missing_indicator);
    for (size_t step = 0; step < stepCount; ++step) {
        bool fixed = fix_missing_value(hydro_vector[step], nearby_good_value, missing_indicator);
        if (fixed) {
            log.push_back("\rWARNING!! Fixing missing hydro vector data at step " + std::to_string(step));
        }
    }
}

