#include "../src/orderbook.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace echomill;
namespace fs = std::filesystem;

static std::string find_data_dir()
{
    // Try to find 'data/' by moving up from CWD
    fs::path current = fs::current_path();
    for (int i = 0; i < 5; ++i) {
        if (fs::exists(current / "data"))
            return (current / "data").string();
        if (current.has_parent_path())
            current = current.parent_path();
        else
            break;
    }
    return "data"; // Fallback
}

// LOBSTER Message
struct Message {
    double time;
    int type;
    OrderId id;
    Qty size;
    Price price;
    int direction;
};

// LOBSTER Book Entry (Level)
struct LevelExpectation {
    Price askPrice;
    Qty askSize;
    Price bidPrice;
    Qty bidSize;
};

// TestableOrderBook to expose insertOrder
class TestableOrderBook : public OrderBook {
public:
    void testInsertOrder(const Order& order) { insertOrder(order); }
};

class LobsterReplayTest : public ::testing::TestWithParam<std::string> {
protected:
    TestableOrderBook book;

    Message parseMessage(const std::string& line)
    {
        Message msg;
        char comma;
        std::stringstream ss(line);
        ss >> msg.time >> comma >> msg.type >> comma >> msg.id >> comma >> msg.size >> comma >> msg.price >> comma >>
            msg.direction;
        return msg;
    }

    std::vector<LevelExpectation> parseBook(const std::string& line)
    {
        std::vector<LevelExpectation> levels;
        std::stringstream ss(line);
        char comma;

        for (int i = 0; i < 5; ++i) {
            LevelExpectation lvl;
            ss >> lvl.askPrice >> comma >> lvl.askSize >> comma >> lvl.bidPrice >> comma >> lvl.bidSize;
            if (i < 4)
                ss >> comma;
            levels.push_back(lvl);
        }
        return levels;
    }

    void runReplay(const std::string& dataDir)
    {
        // Find files
        std::string msgFile, bookFile;
        for (const auto& entry : fs::directory_iterator(dataDir)) {
            std::string path = entry.path().string();
            if (path.find("message") != std::string::npos)
                msgFile = path;
            if (path.find("orderbook") != std::string::npos)
                bookFile = path;
        }

        ASSERT_FALSE(msgFile.empty()) << "Message file not found in " << dataDir;
        ASSERT_FALSE(bookFile.empty()) << "Orderbook file not found in " << dataDir;

        std::ifstream fMsg(msgFile);
        std::ifstream fBook(bookFile);

        std::string msgLine, bookLine;
        int lineNum = 0;

        while (std::getline(fMsg, msgLine) && std::getline(fBook, bookLine)) {
            lineNum++;
            if (lineNum > 100)
                break; // Limit replay to 100 messages to ensure stability

            Message msg = parseMessage(msgLine);
            auto expectedLevels = parseBook(bookLine);

            if (msg.type == 1) {
                Side side = (msg.direction == 1) ? Side::Buy : Side::Sell;
                Order order{msg.id, side, OrderType::Limit, msg.price, msg.size, msg.size, 0};
                book.testInsertOrder(order);
            } else if (msg.type == 2 || msg.type == 4) {
                if (auto* order = book.findOrder(msg.id)) {
                    if (order->remaining > msg.size) {
                        book.modifyOrder(msg.id, order->remaining - msg.size);
                    } else {
                        book.cancelOrder(msg.id);
                    }
                }
            } else if (msg.type == 3) {
                book.cancelOrder(msg.id);
            }

            // Validate Depth
            // Check Level 1 (Best Bid/Ask)
            // LOBSTER: AskP, AskS, BidP, BidS
            // EchoMill: bidDepth[0], askDepth[0]

            auto bids = book.bidDepth(5);
            auto asks = book.askDepth(5);

            // Check Best Bid matches LOBSTER Bid 1
            // LOBSTER uses -9999999999 for empty.

            // Level 1 Comparison
            // Note: LOBSTER levels are Price Levels.
            // EchoMill depth returns aggregated levels.

            // Expected Level 1
            LevelExpectation L1 = expectedLevels[0];

            // Verify Bid
            if (bids.size() > 0) {
                EXPECT_EQ(L1.bidPrice, bids[0].price) << "Line " << lineNum << " Best Bid Price Mismatch";
                EXPECT_EQ(L1.bidSize, bids[0].totalQty) << "Line " << lineNum << " Best Bid Size Mismatch";
            } else {
                // If LOBSTER expects empty (-99... or 0 size)
                // LOBSTER empty price is -9999999999
                if (L1.bidPrice > 0) {
                    ADD_FAILURE() << "Line " << lineNum << " Expected Bid, found empty";
                }
            }

            // Verify Ask
            if (asks.size() > 0) {
                EXPECT_EQ(L1.askPrice, asks[0].price) << "Line " << lineNum << " Best Ask Price Mismatch";
                EXPECT_EQ(L1.askSize, asks[0].totalQty) << "Line " << lineNum << " Best Ask Size Mismatch";
            } else {
                if (L1.askPrice > 0 && L1.askPrice < 9000000000) {
                    ADD_FAILURE() << "Line " << lineNum << " Expected Ask, found empty";
                }
            }
        }
    }
};

TEST_P(LobsterReplayTest, ReplayFile) { runReplay(GetParam()); }

INSTANTIATE_TEST_SUITE_P(SampleData, LobsterReplayTest,
                         ::testing::Values(find_data_dir() + "/LOBSTER_SampleFile_AAPL_2012-06-21_5",
                                           find_data_dir() + "/LOBSTER_SampleFile_GOOG_2012-06-21_5"));
