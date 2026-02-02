# EchoMill Architecture

This document describes the design and architecture of the EchoMill project, a toy implementation of a stock exchange matching engine, built from first principles in modern C++.
---

## What is EchoMill?

EchoMill is a miniature replica of a real stock exchange. Imagine a busy marketplace:
- **Buyers** want to purchase shares at the lowest possible price.
- **Sellers** want to sell shares at the highest possible price.
- The **exchange** sits in the middle, collecting all buy/sell offers (orders) and matching them when a buyer's price meets (or exceeds) a seller's price.

When a match happens, a **trade** occurs. Ownership of shares transfers from seller to buyer at an agreed price.

EchoMill implements this entire workflow:
1. Receive orders (buy or sell, at specific prices and quantities).
2. Attempt to match incoming orders against existing ones.
3. If matched, generate trades; otherwise, store the order in the **order book** (a sorted list of waiting orders).
4. Answer queries like "What's the best current buy/sell price?" or "How much liquidity is available at each price level?"

---

## System Overview

EchoMill is organized into three independent components, each with its own build system:

```
echomill/                 <-- Core matching engine (library + server)
e2etest/                  <-- End-to-end test tool (replay + validation)
client/                   <-- CLI tool for manual interaction
config/                   <-- Shared configuration (instruments)
data/                     <-- Sample data files (LOBSTER format)
```

### Component Responsibilities

| Component   | Role |
|-------------|------|
| **echomill** | The "exchange server." Maintains the order book, processes incoming orders, matches them, generates trades, and answers queries. Runs as a standalone server listening for network requests. |
| **client**   | A command-line tool for humans. Lets you type commands like "BUY 100 shares at $10.50" and see responses. Also supports querying the current order book depth. |
| **e2etest**  | An automated tester. Replays historical order data (from LOBSTER files), sends orders to the running engine, queries the book state, and compares results against a "golden vector" (the known correct book state). |

---

## How They Communicate

All three components can run on **different machines**. They communicate over a network using a simple **HTTP/JSON protocol**:

```
┌──────────────┐       HTTP Request       ┌──────────────────┐
│    client    │ ───────────────────────► │                  │
│   (or curl)  │                          │                  │
│              │ ◄─────────────────────── │                  │
└──────────────┘       JSON Response      │                  │
                                          │     echomill     │
┌──────────────┐       HTTP Request       │   (the server)   │
│   e2etest    │ ───────────────────────► │                  │
│              │                          │                  │
│              │ ◄─────────────────────── │                  │
└──────────────┘       JSON Response      └──────────────────┘
```

**E2E Test Workflow:**
1. **Send order**: `POST /orders` with data from `message.csv` → Server responds with "accepted" and any generated trades.
2. **Query state**: `GET /depth?levels=5` → Server responds with current book state (bids/asks as JSON).
3. **Compare**: e2etest compares the received depth to the corresponding row in `orderbook.csv` (the golden vector).
4. **Repeat** for each message in the file.

**Why HTTP/JSON?**
- Works with `curl` out of the box (no custom client needed for quick tests).
- Trivially routes across machines on a network.
- Easy to document and debug.
- "Good enough" performance for a toy system (real exchanges use binary protocols for ultra-low latency, but that complexity is unnecessary here).

### Example Endpoints

| Endpoint        | Method | Purpose |
|-----------------|--------|---------|
| `/orders`       | POST   | Submit a new order (add, cancel, replace). |
| `/depth?levels=5` | GET  | Query current order book depth (top 5 bid/ask levels). |
| `/trades`       | GET    | Retrieve recent trades. |
| `/status`       | GET    | Health check. |

---

## The Order Book: Heart of the Engine

The **order book** is the central data structure. It maintains two sorted lists:
- **Bids** (buy orders): Sorted by price **descending** (highest price first).
- **Asks** (sell orders): Sorted by price **ascending** (lowest price first).

At each price, multiple orders may exist (different traders). They are queued in **FIFO order** (first-in, first-out)—this is **time priority**.

```
ASKS (sell side)                         
────────────────────────────────         Price goes UP ↑
Price: 10.55  │ 200 shares (Order A)     
Price: 10.52  │ 150 shares (Order B)     
Price: 10.50  │ 100 shares (Order C)     <-- Best Ask (lowest sell price)
══════════════════════════════════════════════════════════════ SPREAD
Price: 10.48  │ 300 shares (Order D)     <-- Best Bid (highest buy price)
Price: 10.45  │ 250 shares (Order E)     
Price: 10.40  │ 100 shares (Order F)     
────────────────────────────────         Price goes DOWN ↓
BIDS (buy side)                          
```

### Matching Logic (Simplified)

When a new **aggressive order** arrives (e.g., a market buy order or a limit buy above the best ask):
1. Start at the **best ask** (lowest sell price).
2. Match as much quantity as possible at that price.
3. If the order isn't fully filled, move to the next price level.
4. Repeat until the order is filled or no more matching prices exist.
5. Any remaining quantity becomes a **passive order** and rests in the book.

Each match generates a **Trade** record.

### Price Discovery: How Trade Prices Are Determined

In an order-driven market, there's no central authority setting prices. Instead, the **trade price is determined by the passive order** — the order that was already resting in the book.

**Example:**
```
Order Book:
  Best Ask: $10.50 (100 shares, Order A)
  Best Bid: $10.45 (200 shares, Order B)

Incoming: Limit Buy at $10.55 for 50 shares (aggressive)
```

Even though the buyer is willing to pay $10.55, the trade executes at **$10.50** because:
- Order A (the passive seller) only asked for $10.50.
- The aggressive buyer gets a better deal than their limit.
- **The passive order's price always wins.**

| Scenario | Trade Price |
|----------|-------------|
| Market order crosses the book | Passive order's price |
| Limit order crosses the book | Passive order's price (buyer/seller may get better than their limit) |

**Why this matters:**
- The "market price" you see quoted is simply the **best bid/ask** in the book.
- Prices emerge naturally from supply and demand (the orders people submit).
- No central pricing authority — pure price discovery.

---

## First Principles: Core Design Decisions

### 1. Price-Time Priority
This is the golden rule of most exchanges. Orders are matched by:
1. **Best price first**: Buy orders with the highest price match before lower ones; sell orders with the lowest price match first.
2. **Time priority**: At the same price, orders that arrived earlier are matched first (FIFO queue per price level).

### 2. Data Structure Choice
For a toy engine, we use:
- **`std::map<Price, std::list<Order>>`** for each side (bids use `std::greater` for descending order).
- The map provides O(log N) lookup/insert by price.
- The list at each price provides O(1) append and FIFO iteration.

For higher performance (and to learn optimization), you can later switch to:
- **Sorted `std::vector`** + binary search (better cache locality).
- **Skip lists** or **flat maps** for predictable latency.

### 3. Fixed-Point Prices
Real engines avoid floating-point for prices (imprecision, non-determinism). Instead:
- Store prices as **integers** (e.g., dollars × 10000, so $10.52 = 105200).
- LOBSTER data already uses this convention (prices multiplied by 10000).

### 4. Minimal Allocations in the Hot Path
Every `new` or `malloc` has overhead. For high performance:
- Reuse order objects via an **object pool**.
- Pre-reserve container capacities.
- Avoid `std::string` copies; use IDs (integers) where possible.

### 5. No Magic Numbers
All constants (tick sizes, buffer sizes, timeouts) are defined as named `constexpr` values, per the coding guidelines.

---

## Testing Strategy

### Unit Tests (in `echomill/test/`)
- Test the `OrderBook` class directly (no network).
- Fast, isolated, run in milliseconds.
- Cover edge cases: empty book, single order, partial fills, cancellations, price-time priority violations.

### Library-Level End-to-End Test (also in `echomill/test/`)
- A "mini E2E": feed a series of orders directly into the `OrderBook` class, then verify the resulting depth and generated trades.
- No network, no server process—just the core logic.

### Full End-to-End Test (in `e2etest/`)
- Spawns the `echomill` server as a separate process.
- Connects over HTTP, replays messages from LOBSTER `message.csv`.
- After each message, queries `/depth` and compares to the corresponding row in LOBSTER `orderbook.csv` (the **golden vector**).
- If any mismatch: test fails, indicating a bug in matching logic.

This layered approach catches bugs early (unit tests) and validates the entire stack (E2E).

---

## Data Format: LOBSTER

EchoMill uses [LOBSTER](https://lobsterdata.com/) sample data for testing. LOBSTER provides:

### `message.csv` — The Order Stream
Each row is an event (order add, cancel, execution, etc.):
| Column    | Meaning |
|-----------|---------|
| Time      | Seconds after midnight (with milliseconds/nanoseconds precision). |
| Type      | 1=Add, 2=Cancel (partial), 3=Delete, 4=Execute (visible), 5=Execute (hidden), 7=Halt. |
| Order ID  | Unique identifier. |
| Size      | Number of shares. |
| Price     | Dollars × 10000 (e.g., 105200 = $10.52). |
| Direction | 1=Buy, -1=Sell. |

### `orderbook.csv` — The Golden Vector
Each row is the book state **after** the corresponding message:
| Columns | Meaning |
|---------|---------|
| Ask Price 1, Ask Size 1, Bid Price 1, Bid Size 1, ... | Top-of-book (level 1). |
| Ask Price 2, Ask Size 2, Bid Price 2, Bid Size 2, ... | Level 2, and so on up to the depth (5 levels for our project). |

By comparing our engine's output to `orderbook.csv`, we verify correctness.

---

## Instruments Configuration

Instruments (tradable symbols) are defined in `config/instruments.json`:

```json
[
  {
    "symbol": "AAPL",
    "tick_size": 0.01,
    "lot_size": 1,
    "price_scale": 10000,
    "description": "Apple Inc."
  },
  {
    "symbol": "GOOG",
    "tick_size": 0.01,
    "lot_size": 1,
    "price_scale": 10000,
    "description": "Alphabet Inc."
  }
]
```

- **symbol**: Short identifier.
- **tick_size**: Minimum price increment (e.g., $0.01).
- **lot_size**: Minimum quantity increment.
- **price_scale**: Multiplier for fixed-point representation (matches LOBSTER convention).

The engine loads this file at startup to validate incoming orders. The client and e2etest can also load it for display and validation purposes.

---

## Build System

Each component has its own `CMakeLists.txt` and can be built independently:

```bash
./make.sh echomill    # Build the core engine
./make.sh client      # Build the CLI tool
./make.sh e2etest     # Build the E2E tester
./make.sh all         # Build everything
./make.sh clean       # Remove all build artifacts
```

No root-level `CMakeLists.txt` is used; the shell script orchestrates builds.

---

## Directory Roles

| Path | Purpose |
|------|---------|
| `echomill/src/` | Core engine source code (OrderBook, server, types). |
| `echomill/test/` | Unit tests and mini library-level E2E tests. |
| `client/src/` | CLI tool source code. |
| `client/test/` | CLI component tests. |
| `e2etest/src/` | E2E tester source code (replayer, comparator). |
| `e2etest/test/` | E2E infrastructure tests (e.g., CSV parser tests). |
| `config/` | Shared configuration (instruments.json). |
| `data/` | Sample LOBSTER data (gitignored). |

---

## Key Classes (Conceptual)

```cpp
// Core types
struct Order {
    OrderId id;
    Side side;          // Buy or Sell
    Price price;        // Fixed-point integer
    Qty qty;            // Total quantity
    Qty remaining;      // Quantity still open
    Timestamp ts;       // Arrival time
};

struct Trade {
    OrderId buyerId;
    OrderId sellerId;
    Price price;
    Qty qty;
    Timestamp ts;
};

// The order book
class OrderBook {
public:
    std::vector<Trade> addOrder(Order order);   // Match and insert
    bool cancelOrder(OrderId id);
    std::optional<Price> bestBid() const;
    std::optional<Price> bestAsk() const;
    std::vector<BookLevel> depth(Side side, size_t levels) const;
};

// Server (wraps OrderBook, listens on HTTP)
class Server {
public:
    void run(uint16_t port);
    // Endpoints: /orders, /depth, /trades, /status
};

// Instrument manager
class InstrumentManager {
public:
    void loadFromFile(const std::string& path);
    const Instrument* find(const std::string& symbol) const;
};
```

---

## Summary

EchoMill is a from-scratch toy matching engine demonstrating:
1. **Order book mechanics** with price-time priority.
2. **Clean architecture**: Separate components for engine, testing, and CLI.
3. **Network-ready design**: HTTP/JSON protocol, curl-compatible.
4. **Rigorous testing**: Unit tests, library-level E2E, and full E2E with golden vector validation.
5. **High-performance thinking**: Fixed-point prices, minimal allocations, cache-friendly structures.

This project mirrors the core logic of real exchange systems like Millennium Exchange, providing both a deep learning experience and a compelling portfolio piece.
