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
    InstrumentManager instruments;
    std::unique_ptr<TestableServer> server;
    int sv[2]; // Socket pair: sv[0] user side, sv[1] server side

    void SetUp() override
    {
        // Setup mock instruments
        instruments.addInstrument({"AAPL", "Apple Inc.", 100, 1, 10000});
        instruments.addInstrument({"GOOG", "Alphabet Inc.", 100, 1, 10000});

        server = std::make_unique<TestableServer>(instruments);

        // Create socket pair
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
    }

    void TearDown() override
    {
        close(sv[0]);
        close(sv[1]);
    }

    // Helper to send request and read response
    std::string sendRequest(const std::string& method, const std::string& path, const std::string& body = "")
    {
        std::stringstream ss;
        ss << method << " " << path << " HTTP/1.1\r\n";
        if (!body.empty()) {
            ss << "Content-Length: " << body.size() << "\r\n";
        }
        ss << "\r\n" << body;

        std::string req = ss.str();
        write(sv[0], req.c_str(), req.size());
        server->testHandleClient(sv[1]);

        char buffer[4096] = {0};
        ssize_t n = read(sv[0], buffer, 4096);
        if (n > 0)
            return std::string(buffer, n);
        return "";
    }

    int getStatus(const std::string& resp)
    {
        std::stringstream ss(resp);
        std::string proto;
        int code;
        ss >> proto >> code;
        return code;
    }
};

TEST_F(ServerTest, AddOrder)
{
    std::string body = "{\"symbol\": \"AAPL\", \"side\": 1, \"price\": 15000, \"qty\": 10, \"id\": 101, \"type\": 1}";
    std::string resp = sendRequest("POST", "/orders", body);
    EXPECT_EQ(getStatus(resp), 200);
    EXPECT_TRUE(resp.find("accepted") != std::string::npos);
}

TEST_F(ServerTest, InvalidSymbol)
{
    std::string body = "{\"symbol\": \"UNKNOWN\", \"side\": 1, \"price\": 15000, \"qty\": 10, \"id\": 102}";
    std::string resp = sendRequest("POST", "/orders", body);
    EXPECT_EQ(getStatus(resp), 400);
}

TEST_F(ServerTest, CancelOrder)
{
    sendRequest("POST", "/orders",
                "{\"symbol\": \"AAPL\", \"side\": 1, \"price\": 14000, \"qty\": 100, \"id\": 201, \"type\": 1}");
    std::string resp = sendRequest("DELETE", "/orders", "{\"id\": 201}");
    EXPECT_EQ(getStatus(resp), 200);
    EXPECT_TRUE(resp.find("cancelled") != std::string::npos);
}

TEST_F(ServerTest, GetDepth)
{
    sendRequest("POST", "/orders",
                "{\"symbol\": \"AAPL\", \"side\": 1, \"price\": 10000, \"qty\": 50, \"id\": 301, \"type\": 1}");
    std::string resp = sendRequest("GET", "/depth?symbol=AAPL&levels=1");
    EXPECT_EQ(getStatus(resp), 200);
    EXPECT_TRUE(resp.find("\"bids\":") != std::string::npos);
    EXPECT_TRUE(resp.find("10000") != std::string::npos);
}

TEST_F(ServerTest, CrossInstrumentIsolation)
{
    // Add buy for AAPL
    sendRequest("POST", "/orders",
                "{\"symbol\": \"AAPL\", \"side\": 1, \"price\": 10000, \"qty\": 50, \"id\": 401, \"type\": 1}");

    // Add sell for GOOG at same price - should NOT match
    std::string respSell =
        sendRequest("POST", "/orders",
                    "{\"symbol\": \"GOOG\", \"side\": -1, \"price\": 10000, \"qty\": 50, \"id\": 402, \"type\": 1}");
    EXPECT_EQ(getStatus(respSell), 200);
    EXPECT_TRUE(respSell.find("\"trades\": []") != std::string::npos);

    // Verify AAPL book still has the bid
    std::string respDepth = sendRequest("GET", "/depth?symbol=AAPL&levels=1");
    EXPECT_TRUE(respDepth.find("10000") != std::string::npos);
}

TEST_F(ServerTest, GetTrades)
{
    std::string resp = sendRequest("GET", "/trades");
    EXPECT_EQ(getStatus(resp), 200);
}

TEST_F(ServerTest, GetStatus)
{
    std::string resp = sendRequest("GET", "/status");
    EXPECT_EQ(getStatus(resp), 200);
}

TEST_F(ServerTest, NotFound)
{
    std::string resp = sendRequest("GET", "/nothing");
    EXPECT_EQ(getStatus(resp), 404);
}
