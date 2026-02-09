#pragma once

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace echomill::json {

// Simple JSON value extraction (no external dependencies)
// This is intentionally minimal - handles only simple key-value pairs

inline std::string extractString(const std::string& json, const std::string& key);
inline int64_t extractInt(const std::string& json, const std::string& key);
inline int64_t extractFixedPoint(const std::string& json, const std::string& key, int multiplier);

struct JsonObject {
    std::string rawJson;

    [[nodiscard]] std::string getString(const std::string& key) const { return extractString(rawJson, key); }

    [[nodiscard]] int64_t getInt(const std::string& key) const { return extractInt(rawJson, key); }

    [[nodiscard]] int64_t getFixedPoint(const std::string& key, int multiplier) const
    {
        return extractFixedPoint(rawJson, key, multiplier);
    }
};

inline std::string extractString(const std::string& json, const std::string& key)
{
    std::string searchKey = "\"" + key + "\"";
    auto keyPos = json.find(searchKey);
    if (keyPos == std::string::npos)
        return "";

    auto colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos)
        return "";

    auto startQuote = json.find('"', colonPos);
    if (startQuote == std::string::npos)
        return "";

    auto endQuote = json.find('"', startQuote + 1);
    if (endQuote == std::string::npos)
        return "";

    return json.substr(startQuote + 1, endQuote - startQuote - 1);
}

inline int64_t extractInt(const std::string& json, const std::string& key)
{
    std::string searchKey = "\"" + key + "\"";
    auto keyPos = json.find(searchKey);
    if (keyPos == std::string::npos)
        return 0;

    auto colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos)
        return 0;

    auto valueStart = colonPos + 1;
    while (valueStart < json.size() && std::isspace(json[valueStart]))
        ++valueStart;

    std::string numStr;
    while (valueStart < json.size() &&
           (std::isdigit(json[valueStart]) || json[valueStart] == '.' || json[valueStart] == '-')) {
        numStr += json[valueStart];
        ++valueStart;
    }

    if (numStr.empty())
        return 0;

    auto dotPos = numStr.find('.');
    if (dotPos != std::string::npos) {
        double value = std::stod(numStr);
        return static_cast<int64_t>(value * 100);
    }

    return std::stoll(numStr);
}

inline int64_t extractFixedPoint(const std::string& json, const std::string& key, int multiplier)
{
    std::string searchKey = "\"" + key + "\"";
    auto keyPos = json.find(searchKey);
    if (keyPos == std::string::npos)
        return 0;

    auto colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos)
        return 0;

    auto valueStart = colonPos + 1;
    while (valueStart < json.size() && std::isspace(json[valueStart]))
        ++valueStart;

    std::string numStr;
    while (valueStart < json.size() &&
           (std::isdigit(json[valueStart]) || json[valueStart] == '.' || json[valueStart] == '-')) {
        numStr += json[valueStart];
        ++valueStart;
    }

    if (numStr.empty())
        return 0;

    return static_cast<int64_t>(std::stod(numStr) * multiplier);
}

inline std::vector<JsonObject> parseArray(const std::string& json)
{
    std::vector<JsonObject> objects;
    size_t pos = 0;
    while (pos < json.size()) {
        auto objectStart = json.find('{', pos);
        if (objectStart == std::string::npos) {
            break;
        }

        auto objectEnd = json.find('}', objectStart);
        if (objectEnd == std::string::npos) {
            break;
        }

        objects.push_back({json.substr(objectStart, objectEnd - objectStart + 1)});
        pos = objectEnd + 1;
    }
    return objects;
}

} // namespace echomill::json
