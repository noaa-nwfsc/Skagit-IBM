#include "model_config_map.h"
#include <stdexcept>

const std::unordered_map<ModelParamKey, std::string>& ModelConfigMap::getFileKeyMap() {
    static std::unordered_map<ModelParamKey, std::string> keyMap;
    if (keyMap.empty()) {
        for (const auto& [key, def] : getModelConfigDefinitions()) {
            keyMap[key] = def.first;
        }
    }
    return keyMap;
}

const std::unordered_map<ModelParamKey, ConfigValue>&
ModelConfigMap::getDefaultValues() {
    static std::unordered_map<ModelParamKey, ConfigValue> defaults;
    if (defaults.empty()) {
        for (const auto& [key, def] : getModelConfigDefinitions()) {
            defaults[key] = def.second;
        }
    }
    return defaults;
}

ModelConfigMap::ModelConfigMap() : paramValues_(getDefaultValues()) {}

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
    const auto& keyMap = getFileKeyMap();

    for (const auto& [configKey, fileKey] : keyMap) {
        if (d.HasMember(fileKey.c_str())) {
            const auto& jsonValue = d[fileKey.c_str()];

            // Determine expected type from default value
            const auto& defaultValue = getDefaultValues().at(configKey);

            if (std::holds_alternative<int>(defaultValue) && jsonValue.IsInt()) {
                set(configKey, jsonValue.GetInt());
            } else if (std::holds_alternative<float>(defaultValue) && jsonValue.IsNumber()) {
                set(configKey, jsonValue.GetFloat());
            } else if (std::holds_alternative<std::string>(defaultValue) && jsonValue.IsString()) {
                set(configKey, std::string(jsonValue.GetString()));
            }
        }
    }
}

std::string ModelConfigMap::getFileKey(ModelParamKey key) const {
    const auto& keyMap = getFileKeyMap();
    auto it = keyMap.find(key);
    return (it != keyMap.end()) ? it->second : "unknown";
}