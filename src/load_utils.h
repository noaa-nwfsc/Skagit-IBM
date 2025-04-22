//
// Created by Troy Frever on 4/21/25.
//

#ifndef LOAD_UTILS_H
#define LOAD_UTILS_H

bool fix_missing_value(float& cell, float& last_good_value, float missing_indicator);
float find_first_non_missing_value(const std::vector<float> &values, float missing_indicator);

#endif //LOAD_UTILS_H
