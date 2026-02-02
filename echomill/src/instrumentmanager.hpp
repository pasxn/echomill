#pragma once

#include "instrument.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace echomill {

class InstrumentManager {
public:
    // Load instruments from JSON file
    void loadFromFile(const std::string& path);

    // Add instrument manually (for testing)
    void addInstrument(Instrument instrument);

    // Lookup by symbol
    [[nodiscard]] const Instrument* find(const std::string& symbol) const;

    // Get all symbols
    [[nodiscard]] std::vector<std::string> allSymbols() const;

    // Clear all instruments
    void clear();

    // Get instrument count
    [[nodiscard]] size_t count() const { return m_instruments.size(); }

private:
    std::unordered_map<std::string, Instrument> m_instruments;
};

} // namespace echomill
