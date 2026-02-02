#include "server.hpp"
#include "jsonutils.hpp"
#include "types.hpp"

#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace echomill {

using namespace json;

Server::Server(const InstrumentManager& instruments) : m_instruments(instruments), m_running(false), m_serverSocket(-1)
{
    // Initialize one OrderBook per instrument
    for (const auto& symbol : m_instruments.allSymbols()) {
        m_books.emplace(std::piecewise_construct, std::forward_as_tuple(symbol), std::forward_as_tuple());
    }
}

void Server::run(uint16_t port)
{
    m_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverSocket < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return;
    }

    int opt = 1;
    setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(m_serverSocket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Failed to bind to port " << port << std::endl;
        return;
    }

    if (listen(m_serverSocket, 10) < 0) {
        std::cerr << "Failed to listen" << std::endl;
        return;
    }

    m_running = true;
    std::cout << "Server listening on port " << port << std::endl;

    while (m_running) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(m_serverSocket, (struct sockaddr*)&clientAddr, &clientLen);

        if (clientSocket < 0) {
            if (m_running)
                std::cerr << "Accept failed" << std::endl;
            continue;
        }

        handleClient(clientSocket);
        close(clientSocket);
    }

    if (m_serverSocket >= 0) {
        close(m_serverSocket);
        m_serverSocket = -1;
    }
}

void Server::stop()
{
    m_running = false;
    // Close server socket to break accept loop
    if (m_serverSocket >= 0) {
        shutdown(m_serverSocket, SHUT_RDWR);
        close(m_serverSocket);
        m_serverSocket = -1;
    }
}

void Server::handleClient(int clientSocket)
{
    std::string request;
    char buffer[4096];
    bool headerComplete = false;
    size_t bodyTarget = 0;
    size_t bodyRead = 0;

    // Read headers
    while (!headerComplete) {
        ssize_t n = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (n <= 0)
            return;
        request.append(buffer, n);

        auto pos = request.find("\r\n\r\n");
        if (pos != std::string::npos) {
            headerComplete = true; // Fixed case

            // Extract Content-Length if present
            auto clPos = request.find("Content-Length: ");
            if (clPos != std::string::npos) {
                auto clEnd = request.find("\r\n", clPos);
                std::string clVal = request.substr(clPos + 16, clEnd - (clPos + 16));
                bodyTarget = std::stoul(clVal);
            }

            bodyRead = request.size() - (pos + 4);
        }
    }

    // Read remaining body
    while (bodyRead < bodyTarget) {
        size_t toRead = std::min((size_t)sizeof(buffer), bodyTarget - bodyRead);
        ssize_t n = recv(clientSocket, buffer, toRead, 0);
        if (n <= 0)
            break;
        request.append(buffer, n);
        bodyRead += n;
    }

    auto doubleCRLF = request.find("\r\n\r\n");
    std::string body = request.substr(doubleCRLF + 4);

    // RESTORED: Parse Request Line (e.g., "POST /orders HTTP/1.1")
    std::istringstream stream(request);
    std::string method, path, protocol;
    stream >> method >> path >> protocol;

    // RESTORED: Separate Query String
    std::string queryString;
    auto quesPos = path.find('?');
    if (quesPos != std::string::npos) {
        queryString = path.substr(quesPos + 1);
        path = path.substr(0, quesPos);
    }

    std::string response;
    try {
        if (path == "/orders") {
            if (method == "POST") {
                response = handleAddOrder(body);
            } else if (method == "DELETE") {
                response = handleCancelOrder(body);
            } else {
                response = createResponse(405, "{\"error\": \"Method Not Allowed\"}");
            }
        } else if (path == "/depth" && method == "GET") {
            response = handleGetDepth(queryString);
        } else if (path == "/trades" && method == "GET") {
            response = handleGetTrades();
        } else if (path == "/status" && method == "GET") {
            response = handleStatus();
        } else {
            response = createResponse(404, "{\"error\": \"Not Found\"}");
        }
    } catch (const std::exception& e) {
        response = createResponse(500, "{\"error\": \"" + std::string(e.what()) + "\"}");
    }

    send(clientSocket, response.c_str(), response.size(), 0);
}

std::string Server::handleAddOrder(const std::string& body)
{
    std::string symbol = extractString(body, "symbol");
    const Instrument* instr = m_instruments.find(symbol);
    if (!instr) {
        return createResponse(400, "{\"error\": \"Unknown symbol\"}");
    }

    Order order{};
    order.id = extractInt(body, "id");
    int sideInput = extractInt(body, "side");
    if (sideInput == -1)
        order.side = Side::Sell;
    else
        order.side = Side::Buy;

    order.price = extractInt(body, "price");
    order.qty = extractInt(body, "qty");

    int type = extractInt(body, "type");
    if (type == 1)
        order.type = OrderType::Limit;
    else
        order.type = OrderType::Market;

    order.remaining = order.qty;
    auto& book = m_books.at(symbol);
    auto trades = book.addOrder(order);

    std::stringstream ss;
    ss << "{\"status\": \"accepted\", \"trades\": [";
    for (size_t i = 0; i < trades.size(); ++i) {
        const auto& t = trades[i];
        ss << "{\"price\": " << t.price << ", \"qty\": " << t.qty << ", \"makerId\": " << t.makerOrderId
           << ", \"takerId\": " << t.takerOrderId << "}";
        if (i < trades.size() - 1)
            ss << ",";
    }
    ss << "]}";
    return createResponse(200, ss.str());
}

std::string Server::handleCancelOrder(const std::string& body)
{
    OrderId id = extractInt(body, "id");
    // Search across all books for the order (simplification for now)
    // In a real system, the request would usually include the symbol.
    for (auto& [symbol, book] : m_books) {
        if (book.cancelOrder(id)) {
            return createResponse(200, "{\"status\": \"cancelled\"}");
        }
    }
    return createResponse(404, "{\"error\": \"Order not found\"}");
}

std::string Server::handleGetDepth(const std::string& queryString)
{
    int levels = 5;
    // Parse query string for levels=... (naive parsing)
    // "levels=10"
    std::string symbol = getQueryParam(queryString, "symbol");
    if (symbol.empty() || m_books.find(symbol) == m_books.end()) {
        return createResponse(400, "{\"error\": \"Invalid or missing symbol\"}");
    }

    auto& book = m_books.at(symbol);
    auto bids = book.bidDepth(levels);
    auto asks = book.askDepth(levels);

    std::stringstream ss;
    ss << "{\"bids\": [";
    for (size_t i = 0; i < bids.size(); ++i) {
        ss << "{\"price\": " << bids[i].price << ", \"qty\": " << bids[i].totalQty
           << ", \"count\": " << bids[i].orderCount << "}";
        if (i < bids.size() - 1)
            ss << ",";
    }
    ss << "], \"asks\": [";
    for (size_t i = 0; i < asks.size(); ++i) {
        ss << "{\"price\": " << asks[i].price << ", \"qty\": " << asks[i].totalQty
           << ", \"count\": " << asks[i].orderCount << "}";
        if (i < asks.size() - 1)
            ss << ",";
    }
    ss << "]}";

    return createResponse(200, ss.str());
}

std::string Server::handleGetTrades()
{
    // Not implemented fully as we don't store trade history in OrderBook yet (only returned from addOrder)
    // For now return empty list or not supported
    return createResponse(200, "{\"trades\": []}");
}

std::string Server::handleStatus()
{
    size_t totalOrders = 0;
    for (const auto& [symbol, book] : m_books) {
        totalOrders += book.orderCount();
    }
    return createResponse(200, "{\"status\": \"ok\", \"orders\": " + std::to_string(totalOrders) + "}");
}

std::string Server::createResponse(int statusCode, const std::string& body)
{
    std::string statusMsg = "OK";
    if (statusCode == 400)
        statusMsg = "Bad Request";
    if (statusCode == 404)
        statusMsg = "Not Found";
    if (statusCode == 500)
        statusMsg = "Internal Server Error";

    std::stringstream ss;
    ss << "HTTP/1.1 " << statusCode << " " << statusMsg << "\r\n";
    ss << "Content-Type: application/json\r\n";
    ss << "Content-Length: " << body.size() << "\r\n";
    ss << "Connection: close\r\n\r\n";
    ss << body;
    return ss.str();
}

std::string Server::getQueryParam(const std::string& query, const std::string& key)
{
    auto pos = query.find(key + "=");
    if (pos == std::string::npos)
        return "";

    auto start = pos + key.size() + 1;
    auto end = query.find('&', start);
    if (end == std::string::npos)
        return query.substr(start);
    return query.substr(start, end - start);
}

} // namespace echomill
