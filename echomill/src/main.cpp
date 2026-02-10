#include "instrumentmanager.hpp"
#include "orderbook.hpp"
#include "server.hpp"

#include <atomic>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>

using namespace echomill;

// Global pointer for signal handler
std::unique_ptr<Server> g_server;

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
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " <port> <config_path>" << std::endl;
            return 1;
        }

        uint16_t port = static_cast<uint16_t>(std::stoi(argv[1]));
        std::string configPath = argv[2];

        std::cout << "EchoMill Matching Engine" << std::endl;
        std::cout << "Loading config from " << configPath << "..." << std::endl;

        // Initialize components
        InstrumentManager instruments;
        instruments.loadFromFile(configPath);
        std::cout << "Loaded " << instruments.count() << " instruments." << std::endl;
        for (const auto& sym : instruments.allSymbols()) {
            std::cout << " - " << sym << std::endl;
        }

        g_server = std::make_unique<Server>(instruments);

        // Setup signal handling
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);

        // Run server (blocking)
        std::cout << "Starting server on port " << port << "..." << std::endl;
        g_server->run(port);

        std::cout << "Server stopped." << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
