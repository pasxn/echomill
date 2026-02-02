# EchoMill

A high-performance trading engine project.

## Prerequisites

- **C++20 Compiler**: `g++-13` or `clang++-17` (or newer).
- **CMake**: Version 3.10+.
- **Google Test**: Required for unit tests.

### Installing Google Test (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install libgtest-dev
```

## Project Structure

- `echomill/`: Core execution engine.
- `client/`: Trading client application.
- `e2etest/`: End-to-end testing suite.
- `config/`: Configuration files (instruments, etc.).
- `data/`: Sample data files.

## Guidelines

See `docs/codingguidelines.md` for coding standards.

## Building
### End-to-End Testing
EchoMill uses a Python-based scenario testing framework.

**Requirements:**
- Python 3
- Built `echomill_server` binary (see build instructions above)

**Run tests:**
```bash
python3 e2etest/runner.py
```

This will spawn the server, run JSON-defined scenarios from `e2etest/scenarios/`, and validate the results.

## Configuration
### Build EchoMill (Engine & Tests)
```bash
cmake -B echomill/build -S echomill && cmake --build echomill/build -j
```

### Running Tests

Individual tests can be run using Google Test filters.

```bash
# Run all tests
./echomill/build/test/echomill_test

# Run matching logic tests only
./echomill/build/test/echomill_test --gtest_filter="*MatchingTest*"
```

To run a single test case (e.g., `AddOrder` in `ServerTest`):
```bash
./echomill/build/test/echomill_test --gtest_filter="ServerTest.AddOrder"
```
