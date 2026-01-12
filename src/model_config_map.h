
#pragma once

#include <unordered_map>
#include <string>
#include <variant>
#include <memory>
#include <rapidjson/document.h>

#include "util.h"

using ConfigValue = std::variant<int, float, std::string>;
using ConfigDefinition = std::pair<std::string, ConfigValue>;

enum class ModelParamKey {
    DirectionlessEdges,
    rng_seed,
    VirtualNodes,
    HabitatMortalityMultiplier,
    MortMin,
    MortMax,
    GrowthSlope,
    GrowthSlopeNearshore,
    PmaxUpperLimit,
    PmaxUpperLimitNearshore,
    PmaxLowerLimit,
    AgentAwareness,
    MortalityInflectionPoint
};

class ModelConfigMap {
private:
    std::unordered_map<ModelParamKey, ConfigValue> paramValues_;
    std::unordered_map<ModelParamKey, std::string> fileKeyMap_;

    static std::unordered_map<ModelParamKey, ConfigDefinition> createDefaultDefinitions();

public:
    ModelConfigMap();

    int getInt(ModelParamKey key) const;
    float getFloat(ModelParamKey key) const;
    std::string getString(ModelParamKey key) const;

    void set(ModelParamKey key, const ConfigValue& value);
    void loadFromJson(const rapidjson::Document& d);
    std::string getFileKey(ModelParamKey key) const;
    void validate() const;
};