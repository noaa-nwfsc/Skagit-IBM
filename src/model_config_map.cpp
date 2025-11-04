#include "model_config_map.h"

#include <iostream>
#include <ostream>
#include <stdexcept>

std::unordered_map<ModelParamKey, ConfigDefinition> ModelConfigMap::createDefaultDefinitions() {
    return {
        {ModelParamKey::DirectionlessEdges, {"directionlessEdges", 1}},
        {ModelParamKey::VirtualNodes, {"virtualNodes", 1}},
        {ModelParamKey::rng_seed, {"rng_seed", static_cast<int>(GlobalRand::USE_RANDOM_SEED)}},
        {ModelParamKey::HabitatMortalityMultiplier, {"habitatMortalityMultiplier", 2.0f}},
        {ModelParamKey::MortMin, {"mortMin", 0.0005f}},
        {ModelParamKey::MortMax, {"mortMax", 0.002f}},
        {ModelParamKey::GrowthSlope, {"growthSlope", 0.0007f}},
        {ModelParamKey::AgentAwareness, {"agentAwareness", "medium"}}, // options are "low", "medium", and "high"
        {ModelParamKey::MortalityInflectionPoint, {"mortalityInflectionPoint", 500.0f}},
    };
}

ModelConfigMap::ModelConfigMap() {
    auto definitions = createDefaultDefinitions();

    for (const auto& [key, def] : definitions) {
        fileKeyMap_[key] = def.first;
        paramValues_[key] = def.second;
    }
}

int ModelConfigMap::getInt(ModelParamKey key) const {
    auto it = paramValues_.find(key);
    if (it != paramValues_.end()) {
        return std::get<int>(it->second);
    }
    throw std::runtime_error("Config key not found");
}

float ModelConfigMap::getFloat(ModelParamKey key) const {
    auto it = paramValues_.find(key);
    if (it != paramValues_.end()) {
        return std::get<float>(it->second);
    }
    throw std::runtime_error("Config key not found");
}

std::string ModelConfigMap::getString(ModelParamKey key) const {
    auto it = paramValues_.find(key);
    if (it != paramValues_.end()) {
        return std::get<std::string>(it->second);
    }
    throw std::runtime_error("Config key not found");
}

void ModelConfigMap::set(ModelParamKey key, const ConfigValue& value) {
    paramValues_[key] = value;
}

void ModelConfigMap::loadFromJson(const rapidjson::Document& d) {
    for (const auto& [configKey, fileKey] : fileKeyMap_) {
        if (d.HasMember(fileKey.c_str())) {
            const auto& jsonValue = d[fileKey.c_str()];
            const auto& currentValue = paramValues_.at(configKey);

            if (std::holds_alternative<int>(currentValue) && jsonValue.IsInt()) {
                set(configKey, jsonValue.GetInt());
            } else if (std::holds_alternative<float>(currentValue) && jsonValue.IsNumber()) {
                set(configKey, jsonValue.GetFloat());
            } else if (std::holds_alternative<std::string>(currentValue) && jsonValue.IsString()) {
                set(configKey, std::string(jsonValue.GetString()));
            }
        }
    }
    validate();
}

std::string ModelConfigMap::getFileKey(ModelParamKey key) const {
    auto it = fileKeyMap_.find(key);
    return (it != fileKeyMap_.end()) ? it->second : "unknown";
}

void ModelConfigMap::validate() const {
    std::string agentAwareness = getString(ModelParamKey::AgentAwareness);
    if (agentAwareness != "low" && agentAwareness != "medium" && agentAwareness != "high") {
        std::cerr << "Invalid value for AgentAwareness: " << agentAwareness << std::endl;
        throw std::runtime_error("Invalid value for AgentAwareness");
    }
}