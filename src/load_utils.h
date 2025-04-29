//
// Created by Troy Frever on 4/21/25.
//

#ifndef LOAD_UTILS_H
#define LOAD_UTILS_H

#include <netcdf>

class NcVarFillModeInterface {
public:
    virtual void getFillModeParameters(bool &fillActive, float *fillValue) const = 0;

    virtual ~NcVarFillModeInterface() = default;
};

class NetCDFVarFillAdapter : public NcVarFillModeInterface {
public:
    NetCDFVarFillAdapter(const netCDF::NcVar &ncVar) : ncVar_(ncVar) {
    }

    void getFillModeParameters(bool &fillActive, float *fillValue) const override {
        ncVar_.getFillModeParameters(fillActive, fillValue);
    }

private:
    const netCDF::NcVar &ncVar_;
};

bool fix_missing_value(float &cell, float &last_good_value, float missing_indicator);
void validate_required_value(const NcVarFillModeInterface &ncVar, float actual_value, std::string exception_msg);
float find_first_non_missing_value(const std::vector<float> &values, float missing_indicator);
void fix_all_missing_values(size_t stepCount, const NcVarFillModeInterface &nc_var_vector, std::vector<float> &hydro_vector,
                            const std::string &vector_name = "", std::vector<std
                                ::string> *error_log = nullptr);

#endif //LOAD_UTILS_H
