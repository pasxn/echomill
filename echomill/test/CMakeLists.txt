# CMakeLists.txt for e2etest
# Project e2etest depends on echomill_core

cmake_minimum_required(VERSION 3.10)
project(EchoMillTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find GTest
find_package(GTest REQUIRED)

# Create single test executable
add_executable(echomill_test
    test_pricelevel.cpp
    test_orderbook.cpp
    test_matching.cpp
    test_instrumentmanager.cpp
    test_integration.cpp
    test_server.cpp
)

# Link against echomill_core and GTest
target_link_libraries(echomill_test
    PRIVATE
    echomill_core
    GTest::gtest_main
)
