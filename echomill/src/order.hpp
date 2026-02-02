#pragma once

#include "types.hpp"

namespace echomill {

struct Order {
    OrderId id;          // Unique identifier
    Side side;           // Buy or Sell
    OrderType type;      // Limit or Market
    Price price;         // Limit price (0 for market orders)
    Qty qty;             // Original quantity
    Qty remaining;       // Quantity still open
    Timestamp timestamp; // Arrival time (for time priority)

    // Check if order is fully filled
    [[nodiscard]] bool isFilled() const { return remaining == 0; }

    // Reduce remaining quantity (after partial fill)
    void fill(Qty amount) { remaining -= amount; }
};

} // namespace echomill
