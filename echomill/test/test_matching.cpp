#include "../src/orderbook.hpp"
#include <gtest/gtest.h>

using namespace echomill;

class MatchingTest : public ::testing::Test {
protected:
    OrderBook book;
};

TEST_F(MatchingTest, PriceTimePriority)
{
    // 3 Sell orders at same price, different times
    book.addOrder({1, Side::Sell, OrderType::Limit, 10000, 10, 10, 1000}); // Order 1
    book.addOrder({2, Side::Sell, OrderType::Limit, 10000, 10, 10, 2000}); // Order 2
    book.addOrder({3, Side::Sell, OrderType::Limit, 10000, 10, 10, 3000}); // Order 3

    // Aggressive buy for 15
    Order buy{4, Side::Buy, OrderType::Limit, 10000, 15, 15, 4000};
    auto trades = book.addOrder(buy);

    ASSERT_EQ(2, trades.size());

    // First trade: matches Order 1 fully (10 shares)
    EXPECT_EQ(1, trades[0].makerOrderId);
    EXPECT_EQ(10, trades[0].qty);

    // Second trade: matches Order 2 partially (5 shares)
    EXPECT_EQ(2, trades[1].makerOrderId);
    EXPECT_EQ(5, trades[1].qty);

    // Verify book state
    // Order 1 gone, Order 2 partial (5 left), Order 3 full (10 left)
    auto depth = book.askDepth(1);
    EXPECT_EQ(15, depth[0].totalQty); // 5 + 10
    EXPECT_EQ(2, depth[0].orderCount);
}

TEST_F(MatchingTest, MarketOrderSweep)
{
    // Sells at 100, 101, 102
    book.addOrder({1, Side::Sell, OrderType::Limit, 10000, 10, 10, 1000});
    book.addOrder({2, Side::Sell, OrderType::Limit, 10100, 10, 10, 1000});
    book.addOrder({3, Side::Sell, OrderType::Limit, 10200, 10, 10, 1000});

    // Market Buy for 25
    Order marketBuy{4, Side::Buy, OrderType::Market, 0, 25, 25, 2000};
    auto trades = book.addOrder(marketBuy);

    ASSERT_EQ(3, trades.size());

    // Trade 1: 10 @ 10000
    EXPECT_EQ(10, trades[0].qty);
    EXPECT_EQ(10000, trades[0].price);

    // Trade 2: 10 @ 10100
    EXPECT_EQ(10, trades[1].qty);
    EXPECT_EQ(10100, trades[1].price);

    // Trade 3: 5 @ 10200
    EXPECT_EQ(5, trades[2].qty);
    EXPECT_EQ(10200, trades[2].price);

    Qty totalTraded = 0;
    for (const auto& t : trades)
        totalTraded += t.qty;
    EXPECT_EQ(25, totalTraded);
}

TEST_F(MatchingTest, PartialFillPassive)
{
    // Sell 10 @ 10000
    book.addOrder({1, Side::Sell, OrderType::Limit, 10000, 10, 10, 1000});

    // Buy 20 @ 10000 (only 10 available)
    Order buy{2, Side::Buy, OrderType::Limit, 10000, 20, 20, 2000};
    auto trades = book.addOrder(buy);

    ASSERT_EQ(1, trades.size());
    EXPECT_EQ(10, trades[0].qty);

    // Remaining 10 shares of buy order should rest in book
    // Buy price was 10000
    EXPECT_EQ(10000, book.bestBid().value());
    EXPECT_EQ(10, book.bidDepth(1)[0].totalQty);
    EXPECT_EQ(0, book.askLevelCount());
}

TEST_F(MatchingTest, NoMatchPriceMismatch)
{
    // Sell @ 10100
    book.addOrder({1, Side::Sell, OrderType::Limit, 10100, 10, 10, 1000});

    // Buy @ 10000 (too low)
    Order buy{2, Side::Buy, OrderType::Limit, 10000, 10, 10, 2000};
    auto trades = book.addOrder(buy);

    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(10000, book.bestBid().value());
    EXPECT_EQ(10100, book.bestAsk().value());
}
