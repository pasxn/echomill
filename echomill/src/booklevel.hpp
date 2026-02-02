#pragma once

#include "types.hpp"

namespace echomill {

struct BookLevel {
    Price price;    // Price at this level
    Qty totalQty;   // Sum of all order quantities at this price
    int orderCount; // Number of orders at this level
};

} // namespace echomill
