<div align="center">
<h1> EchoMill </h1> 
</div>

EchoMill is a high-performance, from-scratch matching engine designed for multi-instrument trading. It implements core financial exchange principles price-time priority, FIFO matching, and fixed-point precision within a clean, modular C++ architecture.

## Overview

Unlike generic order books, EchoMill is built for the network. It features a standalone C++ engine that exposes a RESTful API, accompanied by a rich Python client and a comprehensive scenario-driven testing suite.

#### Core Features
- **Price-Time Priority**: Strictly deterministic FIFO matching per price level.
- **Multi-Instrument Support**: Isolated order books for various symbols (e.g., AAPL, GOOG).
- **Fixed-Point Precision**: Prices are handled as 64-bit integers to eliminate floating-point non-determinism.
- **RESTful API**: Native HTTP/JSON interface for orders, depth, and status.
- **Scenario Testing**: 15+ advanced E2E scenarios covering market sweeps, isolation, and robustness.

## System Components

- `echomill/`: C++ matching engine and HTTP server.
- `client/`: Interactive Python CLI for trading and book inspection.
- `e2etest/`: Python automation for declarative JSON scenario tests.

## Dependencies
- **Engine**: C++20 compiler (`g++-13`+), CMake, Google Test (for unit tests).
- **Client/Test**: Python 3.9+, `requests` library.

## Quick Start

#### Install dependencies (for testing if required)
```bash
sudo apt update
sudo apt install libgtest-dev cmake
```

#### Build the engine
```bash
# Build core engine and tests
cmake -B echomill/build -S echomill && cmake --build echomill/build -j
```

#### Run the server
The server requires a port and a configuration path.
```bash
./echomill/build/src/echomill_server 8080 config/instruments.json
```

#### Start the interactive client
```bash
# Enter the interactive REPL
python3 client/client.py --port 8080
```
*In the client shell, type `help` to see commands.*

#### Run automated E2E tests
```bash
python3 e2etest/runner.py
```
## License

[MIT](LICENSE)
