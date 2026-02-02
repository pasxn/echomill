# EchoMill Architecture
This document describes the design and architecture of EchoMill, a toy stock exchange matching engine built from first principles in modern C++.

---

## System Overview

EchoMill is a distributed system composed of three core components:

1.  **echomill**: The C++ matching engine server. It maintains isolated order books for multiple instruments, performs price-time matching, and exposes a RESTful API.
2.  **client**: A Python-based interactive CLI tool for manual trading and system inspection.
3.  **e2etest**: A Python-based scenario testing framework for automated regression testing.

---

## Communication (REST API)

All components communicate via HTTP/JSON. The server expects `Content-Length` headers for all POST/DELETE requests.

### Endpoints

| Endpoint | Method | Payload | Purpose |
| :--- | :--- | :--- | :--- |
| `/orders` | POST | `{symbol, side, price, qty, id, type}` | Submit a new limit/market order. |
| `/orders` | DELETE | `{"id": <int>}` | Cancel an existing order by ID. |
| `/depth` | GET | `?symbol=AAPL&levels=5` | Get aggregated bid/ask depth for a symbol. |
| `/trades` | GET | None | Retrieve the last 100 executed trades. |
| `/status` | GET | None | Get system health and total active order count. |

### Data Schemas

**Order Side (`side`):**
- `1`: Buy
- `-1`: Sell

**Order Type (`type`):**
- `1`: Limit
- `2`: Market

**Prices and Quantities:**
- Prices are **integers** (Fixed-point: Dollars × 10000).
- Quantities are **integers** (Shares).

---

## Core Engine Logic

### Price-Time Priority
EchoMill follows standard exchange priority rules:
1.  **Price**: Better prices are matched first (Highest Bid, Lowest Ask).
2.  **Time**: At the same price, orders are matched in FIFO (First-In-First-Out) order.

### Multi-Instrument Isolation
The engine is multi-tenant. Each instrument (e.g., AAPL, GOOG) has its own dedicated `OrderBook` instance. Orders submitted for AAPL will never interact with GOOG orders.

### Execution Pricing
The **passive order** (resting in the book) determines the trade price. An aggressive buyer might get a better price than their limit if a cheaper sell order is already resting.

---

## Build and execution

Refer to the main `README.md` for specific build and execution instructions.
