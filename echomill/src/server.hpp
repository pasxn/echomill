#pragma once

#include "instrumentmanager.hpp"
#include "orderbook.hpp"
#include <cstdint>
#include <string>

namespace echomill {

class Server {
public:
    Server(OrderBook& book, const InstrumentManager& instruments);

    // Start listening on port (blocking)
    void run(uint16_t port);

    // Stop server gracefully (can be called from signal handler)
    void stop();

protected:
    // Handle individual client connection
    void handleClient(int clientSocket);

private:
    // HTTP Handlers
    std::string handleAddOrder(const std::string& body);
    std::string handleCancelOrder(const std::string& body);
    std::string handleGetDepth(const std::string& queryString);
    std::string handleGetTrades();
    std::string handleStatus();

    // Helpers
    std::string createResponse(int statusCode, const std::string& body);
    std::string getQueryParam(const std::string& query, const std::string& key);

    OrderBook& m_book;
    const InstrumentManager& m_instruments;
    volatile bool m_running;
    int m_serverSocket;
};

} // namespace echomill
