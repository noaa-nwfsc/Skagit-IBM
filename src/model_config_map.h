#pragma once

#include <unordered_map>
#include <string>
#include <variant>
#include <rapidjson/document.h>

#include "util.h"

using ConfigValue = std::variant<int, float, std::string>;
using ConfigDefinition = std::pair<std::string, ConfigValue>;

enum class ModelParamKey {
    DirectionlessEdges,
    rng_seed,
    HabitatMortalityMultiplier,
};

static const std::unordered_map<ModelParamKey, ConfigDefinition>& getModelConfigDefinitions() {
    static const std::unordered_map<ModelParamKey, ConfigDefinition> modelConfigDefinitions = {
        {ModelParamKey::DirectionlessEdges, {"directionlessEdges", 0}},
        {ModelParamKey::rng_seed, {"rng_seed", static_cast<int>(GlobalRand::USE_RANDOM_SEED)}},
        {ModelParamKey::HabitatMortalityMultiplier, {"habitatMortalityMultiplier", 2.0f}},
    };
    return modelConfigDefinitions;
}

class ModelConfigMap {
private:
    std::unordered_map<ModelParamKey, ConfigValue> paramValues_;

    static const std::unordered_map<ModelParamKey, std::string>& getFileKeyMap();
    static const std::unordered_map<ModelParamKey, ConfigValue>& getDefaultValues();

public:
    ModelConfigMap();

    int getInt(ModelParamKey key) const;
    float getFloat(ModelParamKey key) const;
    std::string getString(ModelParamKey key) const;

    // Set values (used during config loading)
    void set(ModelParamKey key, const ConfigValue& value);

    // Load configuration from JSON document
    void loadFromJson(const rapidjson::Document& d);

    // Get the file key string for a config key (useful for debugging/logging)
    std::string getFileKey(ModelParamKey key) const;
};