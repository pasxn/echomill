#include "../src/instrumentmanager.hpp"
#include "../src/orderbook.hpp"
#include "../src/server.hpp"
#include <future>
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

using namespace echomill;

// TestableServer wrapper to expose protected methods
class TestableServer : public Server {
public:
    using Server::Server; // Inherit constructors

    // Expose handleClient for testing
    void testHandleClient(int clientSocket) { handleClient(clientSocket); }
};

class ServerTest : public ::testing::Test {
protected:
    OrderBook book;
    InstrumentManager instruments;
    std::unique_ptr<TestableServer> server;
    int sv[2]; // Socket pair: sv[0] user side, sv[1] server side

    void SetUp() override
    {
        // Setup mock instrument
        Instrument apple{"AAPL", "Apple Inc.", 100, 100, 2}; // tick 0.01 (100 scaled)
        instruments.addInstrument(apple);

        server = std::make_unique<TestableServer>(book, instruments);

        // Create socket pair
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
    }

    void TearDown() override
    {
        close(sv[0]);
        close(sv[1]);
    }

    // Helper to send request and read response
    std::string sendRequest(const std::string& req)
    {
        write(sv[0], req.c_str(), req.size());

        // Invoke exposed protected method
        server->testHandleClient(sv[1]);

        char buffer[4096] = {0};
        ssize_t n = read(sv[0], buffer, 4096);
        if (n > 0)
            return std::string(buffer, n);
        return "";
    }

    // Helper to extract status from response
    int getStatus(const std::string& resp)
    {
        // HTTP/1.1 200 OK
        std::stringstream ss(resp);
        std::string proto;
        int code;
        ss >> proto >> code;
        return code;
    }
};

TEST_F(ServerTest, AddOrder)
{
    std::string req = "POST /orders HTTP/1.1\r\n\r\n"
                      "{\"symbol\": \"AAPL\", \"side\": 1, \"price\": 15000, \"qty\": 10, \"id\": 101, \"type\": 1}";

    std::string resp = sendRequest(req);
    EXPECT_EQ(getStatus(resp), 200);
    EXPECT_TRUE(resp.find("accepted") != std::string::npos);

    // Verify order in book
    auto bids = book.bidDepth(1);
    ASSERT_FALSE(bids.empty());
    EXPECT_EQ(bids[0].price, 15000);
    EXPECT_EQ(bids[0].totalQty, 10);
}

TEST_F(ServerTest, InvalidSymbol)
{
    std::string req = "POST /orders HTTP/1.1\r\n\r\n"
                      "{\"symbol\": \"UNKNOWN\", \"side\": 1, \"price\": 15000, \"qty\": 10, \"id\": 102}";

    std::string resp = sendRequest(req);
    EXPECT_EQ(getStatus(resp), 400); // Bad Request
}

TEST_F(ServerTest, CancelOrder)
{
    // Add first
    book.addOrder({201, Side::Buy, OrderType::Limit, 14000, 100, 100, 0});

    std::string req = "POST /orders HTTP/1.1\r\n\r\n"
                      "{\"id\": 201, \"type\": 2}"; // Type 2 = Cancel

    std::string resp = sendRequest(req);
    EXPECT_EQ(getStatus(resp), 200);
    EXPECT_TRUE(resp.find("cancelled") != std::string::npos);
    EXPECT_TRUE(book.bidDepth(1).empty());
}

TEST_F(ServerTest, GetDepth)
{
    book.addOrder({301, Side::Buy, OrderType::Limit, 10000, 50, 50, 0});
    book.addOrder({302, Side::Sell, OrderType::Limit, 10100, 50, 50, 0});

    std::string req = "GET /depth?levels=1 HTTP/1.1\r\n\r\n";
    std::string resp = sendRequest(req);
    EXPECT_EQ(getStatus(resp), 200);
    EXPECT_TRUE(resp.find("\"bids\":") != std::string::npos);
    EXPECT_TRUE(resp.find("10000") != std::string::npos);
    EXPECT_TRUE(resp.find("10100") != std::string::npos);
}

TEST_F(ServerTest, GetTrades)
{
    std::string req = "GET /trades HTTP/1.1\r\n\r\n";
    std::string resp = sendRequest(req);
    EXPECT_EQ(getStatus(resp), 200);
}

TEST_F(ServerTest, GetStatus)
{
    std::string req = "GET /status HTTP/1.1\r\n\r\n";
    std::string resp = sendRequest(req);
    EXPECT_EQ(getStatus(resp), 200);
}

TEST_F(ServerTest, NotFound)
{
    std::string req = "GET /nothing HTTP/1.1\r\n\r\n";
    std::string resp = sendRequest(req);
    EXPECT_EQ(getStatus(resp), 404);
}
