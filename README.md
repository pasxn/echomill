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
