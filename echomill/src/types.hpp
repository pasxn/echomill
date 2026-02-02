#pragma once

#include <cstdint>

namespace echomill {

// Strong typedefs for domain clarity
using Price = int64_t;      // Dollars × 10000 (e.g., $585.33 = 5853300)
using Qty = uint32_t;       // Number of shares
using OrderId = uint64_t;   // Unique order identifier
using Timestamp = uint64_t; // Nanoseconds since epoch

// Side of the order
enum class Side : uint8_t { Buy = 1, Sell = 2 };

// Order type
enum class OrderType : uint8_t { Limit = 1, Market = 2 };

// Internal message codes
enum class MessageType : uint8_t {
    Add = 1,
    CancelPartial = 2,
    Delete = 3,
    ExecuteVisible = 4,
    ExecuteHidden = 5,
    Halt = 7
};

} // namespace echomill
