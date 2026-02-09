#pragma once

#include "types.hpp"

#include <string>

namespace echomill {

struct Instrument {
    std::string symbol;      // "AAPL", "GOOG", etc.
    std::string description; // "Apple Inc."
    Price tickSize;          // Minimum price increment (e.g., 100 = $0.01)
    Qty lotSize;             // Minimum quantity increment
    int priceScale;          // Multiplier for fixed-point (10000)

    // Validate price is on tick
    [[nodiscard]] bool isValidPrice(Price price) const { return (price % tickSize) == 0; }

    // Validate quantity is on lot
    [[nodiscard]] bool isValidQty(Qty qty) const { return (qty % lotSize) == 0; }
};

} // namespace echomill
