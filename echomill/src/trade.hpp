#pragma once

#include "types.hpp"

namespace echomill {

struct Trade {
    OrderId takerOrderId; // Aggressive order
    OrderId makerOrderId; // Passive order (was resting in book)
    Side takerSide;       // Side of the taker
    Price price;          // Execution price (maker's price)
    Qty qty;              // Quantity traded
    Timestamp timestamp;  // Execution time
};

} // namespace echomill
