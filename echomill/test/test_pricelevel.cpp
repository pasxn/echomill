#include "../src/pricelevel.hpp"
#include <gtest/gtest.h>

using namespace echomill;

TEST(PriceLevelTest, AddOrder)
{
    PriceLevel level(100);
    EXPECT_TRUE(level.empty());
    EXPECT_EQ(0, level.orderCount());
    EXPECT_EQ(0, level.totalQty());

    Order order{1, Side::Buy, OrderType::Limit, 100, 10, 10, 1000};
    level.addOrder(order);

    EXPECT_FALSE(level.empty());
    EXPECT_EQ(1, level.orderCount());
    EXPECT_EQ(10, level.totalQty());
    EXPECT_EQ(1, level.front().id);
}

TEST(PriceLevelTest, FIFO)
{
    PriceLevel level(100);

    // Add two orders
    level.addOrder({1, Side::Buy, OrderType::Limit, 100, 10, 10, 1000}); // Order 1
    level.addOrder({2, Side::Buy, OrderType::Limit, 100, 20, 20, 2000}); // Order 2

    EXPECT_EQ(2, level.orderCount());
    EXPECT_EQ(30, level.totalQty());

    // Front should be Order 1 (arrived first)
    EXPECT_EQ(1, level.front().id);
}

TEST(PriceLevelTest, RemoveOrder)
{
    PriceLevel level(100);
    level.addOrder({1, Side::Buy, OrderType::Limit, 100, 10, 10, 1000});
    level.addOrder({2, Side::Buy, OrderType::Limit, 100, 20, 20, 2000});

    // Remove first order
    bool removed = level.removeOrder(1);
    EXPECT_TRUE(removed);

    EXPECT_EQ(1, level.orderCount());
    EXPECT_EQ(20, level.totalQty());
    EXPECT_EQ(2, level.front().id); // Now 2 is front

    // Remove non-existent order
    EXPECT_FALSE(level.removeOrder(999));
}

TEST(PriceLevelTest, ReduceOrder)
{
    PriceLevel level(100);
    level.addOrder({1, Side::Buy, OrderType::Limit, 100, 10, 10, 1000});

    // Partial reduction (10 -> 4)
    bool reduced = level.reduceOrder(1, 6);
    EXPECT_TRUE(reduced);
    EXPECT_EQ(1, level.orderCount());
    EXPECT_EQ(4, level.totalQty());
    EXPECT_EQ(4, level.front().remaining);

    // Full reduction (cancel)
    reduced = level.reduceOrder(1, 4);
    EXPECT_TRUE(reduced);
    EXPECT_TRUE(level.empty());
}

TEST(PriceLevelTest, Match)
{
    PriceLevel level(100); // Ask price 100
    level.addOrder({1, Side::Sell, OrderType::Limit, 100, 50, 50, 1000});

    // Incoming aggressor: Buy 20 @ 100
    Order aggressor{2, Side::Buy, OrderType::Limit, 100, 20, 20, 2000};

    Timestamp now = 5000;
    auto trades = level.match(aggressor, now);

    ASSERT_EQ(1, trades.size());
    EXPECT_EQ(20, trades[0].qty);
    EXPECT_EQ(100, trades[0].price);
    EXPECT_EQ(2, trades[0].takerOrderId);
    EXPECT_EQ(1, trades[0].makerOrderId);

    // Aggressor strictly filled
    EXPECT_TRUE(aggressor.isFilled());

    // Passive order remaining qty reduced
    EXPECT_EQ(30, level.totalQty());
    EXPECT_EQ(1, level.orderCount());
    EXPECT_EQ(30, level.front().remaining);
}
