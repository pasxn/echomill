#include "instrumentmanager.hpp"
#include "orderbook.hpp"
#include "server.hpp"

#include <atomic>
#include <csignal>
#include <iostream>
#include <string>

using namespace echomill;

// Global pointer for signal handler
Server* g_server = nullptr;

void signalHandler(int signum)
{
    if (g_server) {
        std::cout << "\nInterrupt signal (" << signum << ") received. Stopping server..." << std::endl;
        g_server->stop();
    }
}

int main(int argc, char* argv[])
{
    try {
        // Configuration
        std::string configPath = "config/instruments.json";
        uint16_t port = 8080;

        // Parse args (minimal)
        if (argc > 1)
            port = static_cast<uint16_t>(std::stoi(argv[1]));
        if (argc > 2)
            configPath = argv[2];

        std::cout << "EchoMill Matching Engine" << std::endl;
        std::cout << "Loading config from " << configPath << "..." << std::endl;

        // Initialize components
        InstrumentManager instruments;
        instruments.loadFromFile(configPath);
        std::cout << "Loaded " << instruments.count() << " instruments." << std::endl;
        for (const auto& sym : instruments.allSymbols()) {
            std::cout << " - " << sym << std::endl;
        }

        OrderBook orderBook;
        Server server(orderBook, instruments);
        g_server = &server;

        // Setup signal handling
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);

        // Run server (blocking)
        std::cout << "Starting server on port " << port << "..." << std::endl;
        server.run(port);

        std::cout << "Server stopped." << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
