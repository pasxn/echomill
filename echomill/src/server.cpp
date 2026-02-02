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

Server::Server(OrderBook& book, const InstrumentManager& instruments)
    : m_book(book), m_instruments(instruments), m_running(false), m_serverSocket(-1)
{
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
    char buffer[4096] = {0};
    ssize_t valread = read(clientSocket, buffer, 4096);
    if (valread <= 0)
        return;

    std::string request(buffer, valread);

    // Parse Request Line (e.g., "POST /orders HTTP/1.1")
    std::istringstream stream(request);
    std::string method, path, protocol;
    stream >> method >> path >> protocol;

    // Separate Query String
    std::string queryString;
    auto quesPos = path.find('?');
    if (quesPos != std::string::npos) {
        queryString = path.substr(quesPos + 1);
        path = path.substr(0, quesPos);
    }

    std::string body;
    auto doubleCRLF = request.find("\r\n\r\n");
    if (doubleCRLF != std::string::npos) {
        body = request.substr(doubleCRLF + 4);
    }

    std::string response;
    try {
        if (path == "/orders" && method == "POST") {
            int type = extractInt(body, "type");
            if (type == 2 || type == 3) {
                response = handleCancelOrder(body);
            } else {
                response = handleAddOrder(body);
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
    auto trades = m_book.addOrder(order);

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
    bool success = m_book.cancelOrder(id);
    if (success) {
        return createResponse(200, "{\"status\": \"cancelled\"}");
    } else {
        return createResponse(404, "{\"error\": \"Order not found\"}");
    }
}

std::string Server::handleGetDepth(const std::string& queryString)
{
    int levels = 5;
    // Parse query string for levels=... (naive parsing)
    // "levels=10"
    std::string levelStr = getQueryParam(queryString, "levels");
    if (!levelStr.empty())
        levels = std::stoi(levelStr);

    auto bids = m_book.bidDepth(levels);
    auto asks = m_book.askDepth(levels);

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
    return createResponse(200, "{\"status\": \"ok\", \"orders\": " + std::to_string(m_book.orderCount()) + "}");
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
