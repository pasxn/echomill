# EchoMill Coding Guidelines and Naming Conventions

This document defines the coding standards, naming conventions, and style guidelines for the EchoMill codebase.

## General C++ Guidance

- **C++ standard**: C++20 (set in `CMakeLists.txt`).
- **Prefer RAII**: use smart pointers (`std::unique_ptr`, `std::shared_ptr`) instead of raw `new`/`delete`.
- **Use `const` liberally** (parameters and member functions where value doesn't change).
- **Prefer passing large objects** by const reference: `const T&` or by value with `std::move`.
- **Prefer `std::optional`/`std::variant`** for expressiveness over sentinel values.
- **Keep functions short** and single-responsibility. Extract helpers for clarity.
- **Error handling**: where appropriate use `exceptions` or explicit error-return patterns consistently per module.
- **Core domain isolated** using traditional templates (SFINAE/C++20 concepts/enable_if for constraints).
- **Zero-overhead compile-time polymorphism** in hot paths (e.g., event dispatch).
- **Runtime virtuals** limited to plugins for extensibility.
- **Balances isolation/testability** with performance (<5 µs jitter via static dispatch).
- **Uses type traits** for simple, traditional template enforcement.

## File Naming

### Source Files
**Pattern**: `lowercase` (no underscores, no spaces)

**Examples**:
- ✅ `applicationmanager.cpp` / `applicationmanager.hpp`
- ✅ `rtexecutor.cpp` / `rtexecutor.hpp`
- ✅ `aetheronclient.cpp` / `aetheronclient.hpp`
- ❌ `real_time_executor.cpp` (NO underscores)
- ❌ `client_bus.cpp` (NO underscores)

### Test Files
**Pattern**: `test_<component>` where `<component>` is lowercase shortform without underscores

**Examples**:
- ✅ `test_rtexecutor.cpp`
- ✅ `test_aetheronclient.cpp`
- ✅ `test_controller.cpp`
- ❌ `test_real_time_executor.cpp` (component part should have NO underscores)

### File Organization
- Group related code into folders under `src/`.
- Source files: `*.cpp`, `*.c`
- Header files: `*.hpp`, `*.h`

## Headers

- **Prefer `#pragma once`** (used across this codebase).
- **Keep includes minimal** in headers. Prefer forward declarations when possible.
- **Include order** (recommended):
  1. Corresponding header (if implementing a header in a .cpp)
  2. Project headers (relative includes)
  3. Third-party headers
  4. Standard library headers

## Naming Conventions

### Classes and Types
**Pattern**: `PascalCase`

**Examples**:
- ✅ `ApplicationManager`
- ✅ `RealTimeExecutor`
- ✅ `AetheronClient`
- ✅ `IF polymorphism is needed only static polymorphism can be used with concepts`
- ✅ Use C++20 concepts for type constraints (see `Executor` concept in `executorconcept.hpp`)

### Methods and Functions
**Pattern**: `camelCase` (starts with lowercase letter)

**Examples**:
- ✅ `loadComponent()`
- ✅ `runLoop()`
- ✅ `getBus()`
- ✅ `registerService<T>()`
- ❌ `LoadComponent()` (should start with lowercase)
- ❌ `GetBus()` (should start with lowercase)

**Rationale**: Follows Google C++ Style Guide and modern C++ conventions. Provides visual distinction from types (PascalCase).

### Member Variables
**Pattern**: `m_<camelCase>`

**Examples**:
- ✅ `m_executionManager`
- ✅ `m_timestep`
- ✅ `m_services`
- ✅ `m_running`
- ❌ `m_ExecutionManager` (use camelCase after prefix)
- ❌ `running_` (trailing underscore not allowed)

### Static Member Variables
**Pattern**: `s_<camelCase>`

**Examples**:
- ✅ `s_logger`
- ✅ `s_instance`

### Constants
**Pattern**: `ALL_CAPS` with underscores OR `constexpr` with descriptive names in anonymous namespaces

**Examples**:
- ✅ `MAX_JITTER_US`
- ✅ `HEARTBEAT_INTERVAL_MS`
- ✅ `constexpr int CONTROL_LOOP_PERIOD_US = 20000;`
- ✅ `constexpr size_t BUFFER_SIZE = 4096;`

### Local Variables
**Pattern**: Descriptive names, minimum 3 characters

**Examples**:
- ✅ `iterator` (not `it`)
- ✅ `executor` (not `ex`)
- ✅ `builder`
- ✅ `count` (acceptable for simple counters)

## Magic Numbers and Constants

- **NEVER use magic numbers directly in code**.
- **Define all numeric literals** as named constants using `constexpr` in anonymous namespaces:
  ```cpp
  namespace {
  constexpr int CONTROL_LOOP_PERIOD_US = 20000; // 50Hz
  constexpr size_t BUFFER_SIZE = 4096;
  constexpr double CONVERSION_FACTOR = 1000.0;
  }
  ```
- **Exception**: Obvious values like 0, 1, 2 in array indices or simple loops are acceptable.

## Constructor Initialization

- **Always initialize ALL member variables** in constructor initializer lists.
- **Use initializer list order** matching declaration order.
- **Avoid redundant default initializers** (e.g., don't write `m_vector()` for types with default constructors).
- **Example**:
  ```cpp
  class MyClass {
  public:
      MyClass(int value) 
          : m_value(value),                    // Initialize in order
            m_data(std::make_unique<Data>())   // Use initializer list
      {
          // Constructor body for non-initializable work
      }
  private:
      int m_value;
      std::unique_ptr<Data> m_data;
  };
  ```

## Code Style and Formatting

### Braces
**Always use braces** for single-statement if/for/while blocks:
```cpp
// CORRECT
if (condition) {
    doSomething();
}

// WRONG
if (condition)
    doSomething();
```

### Output Streams
- Use `'\n'` instead of `std::endl` for performance (avoid unnecessary flushes).

### Pass by Value
Pass by value and use `std::move` for types that will be stored:
```cpp
// CORRECT
MyClass(std::string name) : m_name(std::move(name)) {}

// Less efficient
MyClass(const std::string& name) : m_name(name) {}
```

## Null Pointer Checks

**Always use explicit nullptr comparisons**:
```cpp
// CORRECT
if (ptr != nullptr) { ... }
if (ptr == nullptr) { ... }

// WRONG
if (ptr) { ... }
if (!ptr) { ... }
```

## Type Conversions

- **Use explicit casts** to avoid narrowing conversions:
  ```cpp
  double value = static_cast<double>(longValue) / 1000.0;
  ```
- **Avoid implicit bool conversions** for pointers and C-style strings.

## Signal Handling

**Always cast `signal()` return values to void** to acknowledge:
```cpp
(void)std::signal(SIGINT, signalHandler);
(void)std::signal(SIGTERM, signalHandler);
```

## Function Parameters

**Name ALL function parameters**, including unused ones:
```cpp
void signalHandler(int /*signal*/) { ... }  // CORRECT
void signalHandler(int) { ... }             // WRONG
```

## Global Variables

- **Avoid non-const global variables** when possible.
- If necessary for signal handling or similar low-level code, document why it's needed.
- **Prefer class static members or singletons** for shared state.

## Virtual Functions in Destructors

- Calling virtual functions in destructors bypasses virtual dispatch (by design).
- This is acceptable but should be documented if the behavior is intentional.

## Concurrency and Thread-Safety

- If you add concurrency, **document required locking** and prefer `std::mutex` + RAII wrappers.
- **Use atomic types** for flags that may be accessed from signal handlers.
- For RT code, avoid dynamic allocation and blocking operations.
