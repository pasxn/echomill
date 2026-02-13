#include "../src/orderbook.hpp"
#include <gtest/gtest.h>

using namespace echomill;

class OrderBookTest : public ::testing::Test {
protected:
    OrderBook book;
};

TEST_F(OrderBookTest, AddLimitOrder)
{
    Order order{1, Side::Buy, OrderType::Limit, 10000, 10, 10, 1000};
    auto trades = book.addOrder(order);

    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(1, book.bidLevelCount());
    EXPECT_EQ(10000, book.bestBid().value());
    EXPECT_FALSE(book.bestAsk().has_value());
    EXPECT_EQ(1, book.orderCount());
}

TEST_F(OrderBookTest, CancelOrder)
{
    book.addOrder({1, Side::Buy, OrderType::Limit, 10000, 10, 10, 1000});
    EXPECT_EQ(1, book.orderCount());

    bool cancelled = book.cancelOrder(1);
    EXPECT_TRUE(cancelled);
    EXPECT_EQ(0, book.orderCount());
    EXPECT_FALSE(book.bestBid().has_value());

    // Cancel non-existent
    EXPECT_FALSE(book.cancelOrder(999));
}

TEST_F(OrderBookTest, ModifyOrder)
{
    book.addOrder({1, Side::Buy, OrderType::Limit, 10000, 10, 10, 1000});

    // Reduce quantity (10 -> 4)
    bool modified = book.modifyOrder(1, 4);
    EXPECT_TRUE(modified);
    EXPECT_EQ(4, book.findOrder(1).remaining);
    EXPECT_EQ(10000, book.bestBid().value());

    // Increase quantity (not allowed)
    EXPECT_FALSE(book.modifyOrder(1, 20));

    // Reduce to 0 (cancel)
    modified = book.modifyOrder(1, 0);
    EXPECT_TRUE(modified);
    EXPECT_EQ(0, book.orderCount());

    // Test non-existent order throws
    EXPECT_THROW((void)book.findOrder(999), std::out_of_range);
}

TEST_F(OrderBookTest, BestBidAskSpread)
{
    book.addOrder({1, Side::Buy, OrderType::Limit, 10000, 10, 10, 1000});
    book.addOrder({2, Side::Sell, OrderType::Limit, 10100, 10, 10, 1000});

    EXPECT_EQ(10000, book.bestBid().value());
    EXPECT_EQ(10100, book.bestAsk().value());
    EXPECT_EQ(100, book.spread().value());
}

TEST_F(OrderBookTest, DepthQuery)
{
    // Add 3 buy orders at different prices
    book.addOrder({1, Side::Buy, OrderType::Limit, 10000, 10, 10, 1000});
    book.addOrder({2, Side::Buy, OrderType::Limit, 9900, 20, 20, 1000});
    book.addOrder({3, Side::Buy, OrderType::Limit, 9800, 30, 30, 1000});

    // Add 1 more at top price
    book.addOrder({4, Side::Buy, OrderType::Limit, 10000, 5, 5, 1000});

    auto depth = book.bidDepth(2);
    ASSERT_EQ(2, depth.size());

    // Level 1: 10000 (qty 15, count 2)
    EXPECT_EQ(10000, depth[0].price);
    EXPECT_EQ(15, depth[0].totalQty);
    EXPECT_EQ(2, depth[0].orderCount);

    // Level 2: 9900 (qty 20, count 1)
    EXPECT_EQ(9900, depth[1].price);
    EXPECT_EQ(20, depth[1].totalQty);
    EXPECT_EQ(1, depth[1].orderCount);
}

TEST_F(OrderBookTest, SimpleMatch)
{
    // Sell 10 @ 10000
    book.addOrder({1, Side::Sell, OrderType::Limit, 10000, 10, 10, 1000});

    // Buy 10 @ 10000
    Order aggressive{2, Side::Buy, OrderType::Limit, 10000, 10, 10, 2000};
    auto trades = book.addOrder(aggressive);

    ASSERT_EQ(1, trades.size());
    EXPECT_EQ(10, trades[0].qty);
    EXPECT_EQ(10000, trades[0].price);

    // Check total traded qty (since aggressive is passed by value, local var isn't updated)
    Qty tradedQty = 0;
    for (const auto& t : trades)
        tradedQty += t.qty;
    EXPECT_EQ(10, tradedQty);

    EXPECT_EQ(0, book.orderCount()); // Both fully filled
}
