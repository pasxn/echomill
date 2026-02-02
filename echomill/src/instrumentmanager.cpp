#include "instrumentmanager.hpp"
#include "jsonutils.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace echomill {

using namespace json;

void InstrumentManager::loadFromFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open instruments file: " + path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();

    // Parse array of instrument objects
    // Find each { ... } block
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

        std::string objectJson = json.substr(objectStart, objectEnd - objectStart + 1);

        Instrument instrument{};
        instrument.symbol = extractString(objectJson, "symbol");
        instrument.description = extractString(objectJson, "description");

        // Use 10000 scaling for tick_size to match price_scale (0.01 -> 100)
        instrument.tickSize = extractFixedPoint(objectJson, "tick_size", 10000);

        instrument.lotSize = static_cast<Qty>(extractInt(objectJson, "lot_size"));
        instrument.priceScale = static_cast<int>(extractInt(objectJson, "price_scale"));

        if (!instrument.symbol.empty()) {
            m_instruments[instrument.symbol] = std::move(instrument);
        }

        pos = objectEnd + 1;
    }
}

void InstrumentManager::addInstrument(Instrument instrument)
{
    m_instruments[instrument.symbol] = std::move(instrument);
}

const Instrument* InstrumentManager::find(const std::string& symbol) const
{
    auto iterator = m_instruments.find(symbol);
    if (iterator == m_instruments.end()) {
        return nullptr;
    }
    return &iterator->second;
}

std::vector<std::string> InstrumentManager::allSymbols() const
{
    std::vector<std::string> symbols;
    symbols.reserve(m_instruments.size());
    for (const auto& [symbol, instrument] : m_instruments) {
        symbols.push_back(symbol);
    }
    return symbols;
}

void InstrumentManager::clear() { m_instruments.clear(); }

} // namespace echomill
