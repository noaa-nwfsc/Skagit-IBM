//
// Created by Troy Frever on 4/21/25.
//
#include <cmath>
#include <vector>
#include <netcdf>

#include "custom_exceptions.h"
#include "load_utils.h"


bool is_missing_indicator(const float value, const float missing_indicator) {
    if (std::isnan(missing_indicator))
        return std::isnan(value);
    return value == missing_indicator;
}

void validate_required_value(const NcVarFillModeInterface &ncVar, float actual_value, std::string exception_msg) {
    bool is_fill_active;
    float missing_indicator;

    ncVar.getFillModeParameters(is_fill_active, &missing_indicator);
    if (is_missing_indicator(actual_value, missing_indicator)) {
        throw MissingRequiredValueException(exception_msg);
    }
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

void fix_all_missing_values(size_t stepCount, const NcVarFillModeInterface &nc_var_vector, std::vector<float> &hydro_vector,
                            const std::string &vector_name, std::vector<std::string> *error_log) {
    bool is_fill_active;
    float missing_indicator;

    nc_var_vector.getFillModeParameters(is_fill_active, &missing_indicator);
    float nearby_good_value = find_first_non_missing_value(hydro_vector, missing_indicator);

    if (stepCount != hydro_vector.size()) {
        throw WrongLengthVectorException(vector_name);
    }
    if (is_missing_indicator(nearby_good_value, missing_indicator)) {
        throw AllMissingValuesException(vector_name);
    }

    for (size_t step = 0; step < stepCount; ++step) {
        bool fixed = fix_missing_value(hydro_vector[step], nearby_good_value, missing_indicator);
        if (fixed && error_log != nullptr) {
            error_log->push_back("WARNING!! Fixing missing vector data in " + vector_name + " at step " + std::to_string(step));
        }
    }
}
