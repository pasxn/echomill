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
e2etest/                  <-- End-to-end test framework
client/                   <-- CLI tool for manual interaction
config/                   <-- Shared configuration (instruments)
```

### Component Responsibilities

| Component   | Role |
|-------------|------|
| **echomill** | The "exchange server." Maintains the order book, processes incoming orders, matches them, generates trades, and answers queries. Runs as a standalone server listening for network requests. |
| **client**   | A command-line tool for humans. Lets you type commands like "BUY 100 shares at $10.50" and see responses. Also supports querying the current order book depth. |
| **e2etest**  | Automated scenario tester. Runs scripted interactions (e.g. "Scenario A: Large Trade") against the server and validates state. |

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

**E2E Test Workflow (Scenario Testing):**
The `e2etest` framework treats the server as a black box. A Python runner spawns the server and executes a sequence of HTTP requests defined in a JSON file, verifying the response and state at each step.

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

At each price, multiple orders may exist (different traders). They are queued in **FIFO order** (first-in, first-out), this is **time priority**.

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

In an order-driven market, there's no central authority setting prices. Instead, the **trade price is determined by the passive order**, the order that was already resting in the book.

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
- No central pricing authority, pure price discovery.

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
- Store prices as **integers** (e.g., dollars × 10000, so $10.52 = 105200). This fixed-point approach ensures precision.

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
- No network, no server process, just the core logic.

### Full End-to-End Test (in `e2etest/`)
- Spawns the `echomill` server and runs automated scenario scripts.
- Verifies complex flows (e.g. self-matching prevention, large order sweeps, partial fills).

This layered approach catches bugs early (unit tests) and validates the entire stack (E2E).

---

## Scenario Testing Framework

EchoMill uses a black-box testing strategy driven by declarative JSON scenarios. This allows testing complex market behaviors (like sweeps, partial fills, and self-matching) without writing compilation-heavy C++ code.

### 1. Test Architecture

```
e2etest/
├── runner.py           # Python test runner
├── scenarios/          # JSON test definitions
│   ├── 01_basic_match.json
│   ├── 02_market_sweep.json
│   └── ...
└── README.md
```

### 2. The Runner (`runner.py`)
The Python runner orchestrates the test session:
1.  **Spawns** a fresh `echomill` server instance on a random ephemeral port.
2.  **Parses** all `.json` scenario files.
3.  **Executes** each scenario sequentially:
    - Sends HTTP requests specified in `steps`.
    - Validates HTTP status codes.
    - Validates JSON response bodies against expectations (subset matching).
4.  **Reports** results and shuts down the server.

### 3. Scenario Definition (JSON)
Each file represents one isolated test case.

**Schema Example:**
```json
{
  "meta": {
    "name": "Market Buy Sweep",
    "description": "Verify that a market order sweeps multiple price levels."
  },
  "setup": {
    "instrument": "AAPL"
  },
  "steps": [
    {
      "name": "Add Passive Sell 1",
      "action": "POST /orders",
      "body": { "symbol": "AAPL", "side": "sell", "type": "limit", "price": 1050, "qty": 10 },
      "expect_status": 200
    },
    {
      "name": "Add Passive Sell 2",
      "action": "POST /orders",
      "body": { "symbol": "AAPL", "side": "sell", "type": "limit", "price": 1055, "qty": 20 },
      "expect_status": 200
    },
    {
      "name": "Execute Market Buy",
      "action": "POST /orders",
      "body": { "symbol": "AAPL", "side": "buy", "type": "market", "qty": 25 },
      "expect_body": {
        "trades": [
          { "price": 1050, "qty": 10 },
          { "price": 1055, "qty": 15 }
        ]
      }
    },
    {
      "name": "Verify Remaining Liquidity",
      "action": "GET /depth?levels=1",
      "expect_body": {
        "asks": [ { "price": 1055, "qty": 5 } ]
      }
    }
  ]
}
```

This declarative approach ensures tests are readable, easily extensible, and decoupled from the C++ implementation.

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
- **price_scale**: Multiplier for fixed-point representation.

The engine loads this file at startup to validate incoming orders.

---

## Summary

EchoMill is a from-scratch toy matching engine demonstrating:
1. **Order book mechanics** with price-time priority.
2. **Clean architecture**: Separate components for engine, testing, and CLI.
3. **Network-ready design**: HTTP/JSON protocol, curl-compatible.
4. **Rigorous testing**: Unit tests, library-level E2E, and scenario-driven full E2E.
5. **High-performance thinking**: Fixed-point prices, minimal allocations, cache-friendly structures.
