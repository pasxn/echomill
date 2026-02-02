#pragma once

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace echomill::json {

// Simple JSON value extraction (no external dependencies)
// This is intentionally minimal - handles only simple key-value pairs

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

    // Skip whitespace after colon
    auto valueStart = colonPos + 1;
    while (valueStart < json.size() && std::isspace(json[valueStart]))
        ++valueStart;

    // Parse number
    std::string numStr;
    while (valueStart < json.size() &&
           (std::isdigit(json[valueStart]) || json[valueStart] == '.' || json[valueStart] == '-')) {
        numStr += json[valueStart];
        ++valueStart;
    }

    if (numStr.empty())
        return 0;

    // Handle decimal values by converting to fixed-point (assumes x100 or x10000 based on context, but here we do x100)
    // Actually, for generic use, let's return raw int or provide a float version.
    // But since our specific use case (prices) needs scaling, we should be careful.
    // The previous implementation in instrumentmanager.cpp handled decimals by * 100.
    // Let's keep it consistent: treat '.' as a separator we might need to handle.

    auto dotPos = numStr.find('.');
    if (dotPos != std::string::npos) {
        // Simple heuristic: if it has a decimal, multiply by 100 to get cents?
        // Wait, LOBSTER prices are x10000. Instruments might be x100 or x10000?
        // In instrumentmanager it was * 100. Let's replicate that behavior.
        double value = std::stod(numStr);
        return static_cast<int64_t>(value * 100);
    }

    return std::stoll(numStr);
}

// Extract a value using a custom multiplier for fixed point conversion
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

} // namespace echomill::json
