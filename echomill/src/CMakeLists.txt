# Source targets for echomill

# Core matching engine library
add_library(echomill_core STATIC
    pricelevel.cpp
    orderbook.cpp
    instrumentmanager.cpp
    server.cpp
)

target_include_directories(echomill_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(echomill_core PUBLIC cxx_std_20)

# Server executable
add_executable(echomill_server
    main.cpp
)

target_link_libraries(echomill_server PRIVATE echomill_core)
