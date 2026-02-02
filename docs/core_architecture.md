# EchoMill Core Architecture

This document provides the detailed software design and class hierarchy for the **echomill** matching engine core. It complements `docs.md` (high-level overview) and adheres to `codingguidelines.md`.

---

## Design Philosophy

1. **Separation of Concerns**: Core matching logic is isolated from networking, serialization, and I/O.
2. **Zero-Cost Abstractions**: Hot paths use templates and compile-time polymorphism (no virtual dispatch).
3. **Value Semantics**: Prefer copying small structs over pointer indirection.
4. **Determinism**: Fixed-point arithmetic for prices; no floating-point in matching logic.
5. **Testability**: All core classes are dependency-injected and unit-testable without networking.

---

## Type System

All fundamental types are defined in `types.hpp`. Using strong typedefs prevents mixing incompatible values.

```cpp
// types.hpp
#pragma once

#include <cstdint>
#include <chrono>

namespace echomill {

// Strong typedefs for domain clarity
using Price     = int64_t;   // Dollars × 10000 (e.g., $585.33 = 5853300)
using Qty       = uint32_t;  // Number of shares
using OrderId   = uint64_t;  // Unique order identifier
using Timestamp = uint64_t;  // Nanoseconds since epoch

// Side of the order
enum class Side : uint8_t {
    Buy  = 1,
    Sell = 2
};

// Order type
enum class OrderType : uint8_t {
    Limit  = 1,
    Market = 2
};

// Message type (from LOBSTER format)
enum class MessageType : uint8_t {
    Add           = 1,
    CancelPartial = 2,
    Delete        = 3,
    ExecuteVisible = 4,
    ExecuteHidden  = 5,
    Halt          = 7
};

} // namespace echomill
```

---

## Core Data Structures

### Order

Represents a single order in the book.

```cpp
// order.hpp
#pragma once

#include "types.hpp"

namespace echomill {

struct Order {
    OrderId   id;           // Unique identifier
    Side      side;         // Buy or Sell
    OrderType type;         // Limit or Market
    Price     price;        // Limit price (0 for market orders)
    Qty       qty;          // Original quantity
    Qty       remaining;    // Quantity still open
    Timestamp timestamp;    // Arrival time (for time priority)

    // Check if order is fully filled
    [[nodiscard]] bool isFilled() const { return remaining == 0; }

    // Reduce remaining quantity (after partial fill)
    void fill(Qty amount) { remaining -= amount; }
};

} // namespace echomill
```

### Trade

Generated when two orders match.

```cpp
// trade.hpp
#pragma once

#include "types.hpp"

namespace echomill {

struct Trade {
    OrderId   takerOrderId;  // Aggressive order
    OrderId   makerOrderId;  // Passive order (was resting in book)
    Side      takerSide;     // Side of the taker
    Price     price;         // Execution price (maker's price)
    Qty       qty;           // Quantity traded
    Timestamp timestamp;     // Execution time
};

} // namespace echomill
```

### BookLevel

Aggregated view of a price level (for depth queries).

```cpp
// booklevel.hpp
#pragma once

#include "types.hpp"

namespace echomill {

struct BookLevel {
    Price price;      // Price at this level
    Qty   totalQty;   // Sum of all order quantities at this price
    int   orderCount; // Number of orders at this level
};

} // namespace echomill
```

---

## Class Hierarchy

```
┌─────────────────────────────────────────────────────────────────┐
│                         echomill/src/                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────┐     ┌─────────────┐     ┌─────────────────┐    │
│  │   types     │     │    order    │     │     trade       │    │
│  │   (.hpp)    │     │   (.hpp)    │     │    (.hpp)       │    │
│  └─────────────┘     └─────────────┘     └─────────────────┘    │
│         │                   │                    │              │
│         └───────────────────┼────────────────────┘              │
│                             ▼                                   │
│                    ┌─────────────────┐                          │
│                    │   PriceLevel    │  ◄── Orders at one price │
│                    │   (.hpp/.cpp)   │                          │
│                    └────────┬────────┘                          │
│                             │                                   │
│                             ▼                                   │
│                    ┌─────────────────┐                          │
│                    │   OrderBook     │  ◄── Core matching engine│
│                    │   (.hpp/.cpp)   │                          │
│                    └────────┬────────┘                          │
│                             │                                   │
│         ┌───────────────────┼───────────────────┐               │
│         ▼                   ▼                   ▼               │
│  ┌─────────────┐   ┌─────────────────┐   ┌─────────────┐        │
│  │ Instrument  │   │  TradeReporter  │   │  OrderIdGen │        │
│  │  Manager    │   │   (callback)    │   │ (optional)  │        │
│  └─────────────┘   └─────────────────┘   └─────────────┘        │
│                                                                 │
│                             │                                   │
│                             ▼                                   │
│                    ┌─────────────────┐                          │
│                    │     Server      │  ◄── HTTP layer          │
│                    │   (.hpp/.cpp)   │      (wraps OrderBook)   │
│                    └─────────────────┘                          │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## PriceLevel Class

Maintains all orders at a single price point, preserving FIFO order (time priority).

```cpp
// pricelevel.hpp
#pragma once

#include "order.hpp"
#include <list>

namespace echomill {

class PriceLevel {
public:
    explicit PriceLevel(Price price);

    // Getters
    [[nodiscard]] Price price() const { return m_price; }
    [[nodiscard]] Qty   totalQty() const { return m_totalQty; }
    [[nodiscard]] int   orderCount() const { return static_cast<int>(m_orders.size()); }
    [[nodiscard]] bool  empty() const { return m_orders.empty(); }

    // Add order to back of queue (time priority)
    void addOrder(Order order);

    // Remove specific order by ID
    bool removeOrder(OrderId id);

    // Match against this level (returns trades, modifies orders)
    // Fills orders front-to-back (FIFO)
    std::vector<Trade> match(Order& aggressiveOrder, Timestamp execTime);

    // Get front order (for inspection)
    [[nodiscard]] const Order& front() const { return m_orders.front(); }

private:
    Price m_price;
    Qty   m_totalQty;
    std::list<Order> m_orders;  // FIFO queue (list for stable iterators)
};

} // namespace echomill
```

**Why `std::list`?**
- Stable iterators (orders can be removed mid-iteration during matching).
- O(1) removal with iterator.
- O(1) push_back for time priority.

---

## OrderBook Class

The heart of the matching engine. Maintains bid and ask sides, performs matching, and generates trades.

```cpp
// orderbook.hpp
#pragma once

#include "order.hpp"
#include "trade.hpp"
#include "booklevel.hpp"
#include "pricelevel.hpp"

#include <map>
#include <unordered_map>
#include <vector>
#include <optional>
#include <functional>

namespace echomill {

class OrderBook {
public:
    // Callback type for trade notifications
    using TradeCallback = std::function<void(const Trade&)>;

    OrderBook() = default;

    // Set optional trade callback (called for each trade)
    void setTradeCallback(TradeCallback callback);

    // Main operations
    std::vector<Trade> addOrder(Order order);
    bool cancelOrder(OrderId id);
    bool modifyOrder(OrderId id, Qty newQty);  // Reduce qty only

    // Queries
    [[nodiscard]] std::optional<Price> bestBid() const;
    [[nodiscard]] std::optional<Price> bestAsk() const;
    [[nodiscard]] std::optional<Price> spread() const;

    // Get depth (top N levels per side)
    [[nodiscard]] std::vector<BookLevel> bidDepth(size_t levels) const;
    [[nodiscard]] std::vector<BookLevel> askDepth(size_t levels) const;

    // Order lookup
    [[nodiscard]] const Order* findOrder(OrderId id) const;

    // Statistics
    [[nodiscard]] size_t bidLevelCount() const { return m_bids.size(); }
    [[nodiscard]] size_t askLevelCount() const { return m_asks.size(); }

private:
    // Match aggressive order against opposite side
    std::vector<Trade> matchOrder(Order& order);

    // Insert passive order into book
    void insertOrder(const Order& order);

    // Remove empty price levels
    void cleanupLevel(Side side, Price price);

    // Bids: sorted descending (highest price first) 
    std::map<Price, PriceLevel, std::greater<Price>> m_bids;

    // Asks: sorted ascending (lowest price first)
    std::map<Price, PriceLevel> m_asks;

    // Fast order lookup by ID (for cancel/modify)
    std::unordered_map<OrderId, std::pair<Side, Price>> m_orderIndex;

    // Optional trade callback
    TradeCallback m_tradeCallback;
};

} // namespace echomill
```

### Matching Algorithm

```
addOrder(order):
    1. If order crosses the book (can match immediately):
       - Call matchOrder(order) → generates trades
    2. If order has remaining quantity and is a limit order:
       - Insert into appropriate side as passive order
    3. Return all generated trades

matchOrder(order):
    trades = []
    oppositeBook = (order.side == Buy) ? m_asks : m_bids

    while order.remaining > 0 AND oppositeBook not empty:
        bestLevel = oppositeBook.front()

        // Check if price is acceptable
        if order is limit AND doesn't cross bestLevel.price:
            break  // No more matches possible

        // Match against this level
        levelTrades = bestLevel.match(order, now())
        trades.append(levelTrades)

        if bestLevel.empty():
            remove bestLevel from oppositeBook

    return trades
```

---

## Instrument Class

Defines tradable instrument metadata. Loaded from `config/instruments.json`.

```cpp
// instrument.hpp
#pragma once

#include "types.hpp"
#include <string>

namespace echomill {

struct Instrument {
    std::string symbol;       // "AAPL", "GOOG", etc.
    std::string description;  // "Apple Inc."
    Price       tickSize;     // Minimum price increment (e.g., 100 = $0.01)
    Qty         lotSize;      // Minimum quantity increment
    int         priceScale;   // Multiplier for fixed-point (10000)

    // Validate price is on tick
    [[nodiscard]] bool isValidPrice(Price price) const {
        return price % tickSize == 0;
    }

    // Validate quantity is on lot
    [[nodiscard]] bool isValidQty(Qty qty) const {
        return qty % lotSize == 0;
    }
};

} // namespace echomill
```

---

## InstrumentManager Class

Loads and manages instrument definitions.

```cpp
// instrumentmanager.hpp
#pragma once

#include "instrument.hpp"
#include <string>
#include <unordered_map>
#include <optional>

namespace echomill {

class InstrumentManager {
public:
    // Load instruments from JSON file
    void loadFromFile(const std::string& path);

    // Lookup by symbol
    [[nodiscard]] const Instrument* find(const std::string& symbol) const;

    // Get all symbols
    [[nodiscard]] std::vector<std::string> allSymbols() const;

private:
    std::unordered_map<std::string, Instrument> m_instruments;
};

} // namespace echomill
```

---

## Server Class (Networking Layer)

Wraps the OrderBook and exposes HTTP endpoints. This is the **only** class with external dependencies (HTTP library).

```cpp
// server.hpp
#pragma once

#include "orderbook.hpp"
#include "instrumentmanager.hpp"
#include <cstdint>
#include <string>

namespace echomill {

class Server {
public:
    Server(OrderBook& book, const InstrumentManager& instruments);

    // Start listening on port
    void run(uint16_t port);

    // Stop server gracefully
    void stop();

private:
    // HTTP handlers
    std::string handleAddOrder(const std::string& body);
    std::string handleCancelOrder(const std::string& body);
    std::string handleGetDepth(int levels);
    std::string handleGetTrades();
    std::string handleStatus();

    OrderBook& m_book;
    const InstrumentManager& m_instruments;
    bool m_running;
};

} // namespace echomill
```

---

## File Structure

```
echomill/
├── CMakeLists.txt              # Builds library + server executable
├── src/
│   ├── CMakeLists.txt
│   ├── types.hpp               # Fundamental types (Price, Qty, OrderId, etc.)
│   ├── order.hpp               # Order struct
│   ├── trade.hpp               # Trade struct
│   ├── booklevel.hpp           # BookLevel struct (for depth queries)
│   ├── pricelevel.hpp          # PriceLevel class
│   ├── pricelevel.cpp
│   ├── orderbook.hpp           # OrderBook class (core matching engine)
│   ├── orderbook.cpp
│   ├── instrument.hpp          # Instrument struct
│   ├── instrumentmanager.hpp   # InstrumentManager class
│   ├── instrumentmanager.cpp
│   ├── server.hpp              # HTTP server wrapper
│   ├── server.cpp
│   └── main.cpp                # Entry point (loads config, starts server)
└── test/
    ├── CMakeLists.txt
    ├── test_pricelevel.cpp     # Unit tests for PriceLevel
    ├── test_orderbook.cpp      # Unit tests for OrderBook
    ├── test_matching.cpp       # Matching logic tests (price-time priority)
    └── test_integration.cpp    # Library-level E2E (no network)
```

---

## Data Flow

```
                    ┌────────────────────┐
                    │   HTTP Request     │
                    │  POST /orders      │
                    └─────────┬──────────┘
                              │
                              ▼
                    ┌────────────────────┐
                    │      Server        │
                    │  (parse JSON)      │
                    └─────────┬──────────┘
                              │
                              ▼
                    ┌────────────────────┐
                    │  InstrumentManager │
                    │  (validate symbol) │
                    └─────────┬──────────┘
                              │
                              ▼
                    ┌────────────────────┐
                    │     OrderBook      │
                    │   addOrder(...)    │
                    └─────────┬──────────┘
                              │
            ┌─────────────────┼─────────────────┐
            ▼                 ▼                 ▼
    ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
    │ matchOrder() │  │ insertOrder()│  │  Trades[]    │
    │ (if crosses) │  │ (if passive) │  │  (returned)  │
    └──────────────┘  └──────────────┘  └──────────────┘
                              │
                              ▼
                    ┌────────────────────┐
                    │   HTTP Response    │
                    │  { trades: [...] } │
                    └────────────────────┘
```

---

## Performance Considerations

### Hot Path Optimizations

| Technique | Benefit |
|-----------|---------|
| `std::map` with `std::greater` for bids | O(log N) insert/lookup, sorted order maintained |
| `std::list` for FIFO at each price | O(1) front removal, stable iterators |
| `std::unordered_map` for order index | O(1) cancel by OrderId |
| Fixed-point `int64_t` prices | No floating-point overhead, deterministic |
| `[[nodiscard]]` on queries | Compiler warnings if results ignored |

### Future Optimizations (Not in Initial Version)

- **Object pool** for Order structs (reduce allocations).
- **Intrusive list** instead of `std::list` (better cache locality).
- **Lock-free queue** for incoming orders (if multi-threaded).
- **Custom allocator** for `std::map` nodes.

---

## Error Handling Strategy

| Scenario | Handling |
|----------|----------|
| Invalid OrderId (cancel/modify) | Return `false`, log warning |
| Invalid price (not on tick) | Reject order, return error response |
| Invalid symbol | Reject order, return error response |
| Quantity modification increases qty | Reject (only reduction allowed) |
| Network errors | Log, respond with 500 status |

---

## Thread Safety (Future)

The initial implementation is **single-threaded**. If concurrency is added later:

1. **Read-write lock** on `OrderBook` (multiple readers for depth, exclusive for add/cancel).
2. **Lock-free queue** between network thread and matching thread.
3. **Atomic flags** for shutdown signaling.

---

## Summary

| Component | Responsibility |
|-----------|----------------|
| `types.hpp` | Strong typedefs for domain types |
| `Order` | Single order representation |
| `Trade` | Execution report |
| `PriceLevel` | FIFO queue at one price |
| `OrderBook` | Matching engine core |
| `Instrument` | Tradable symbol metadata |
| `InstrumentManager` | Config loader |
| `Server` | HTTP wrapper (only external dependency) |

This architecture isolates the matching engine from all I/O, making it easy to unit test, benchmark, and later optimize without touching networking code.
