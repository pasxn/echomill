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

## Building and Testing

EchoMill uses CMake for its build system.
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
