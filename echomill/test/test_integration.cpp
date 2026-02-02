#include "../src/instrumentmanager.hpp"
#include "../src/orderbook.hpp"
#include <gtest/gtest.h>

using namespace echomill;

class IntegrationTest : public ::testing::Test {
protected:
    OrderBook book;
    InstrumentManager instruments;

    void SetUp() override
    {
        // Manually add instrument for deterministic testing
        Instrument aapl{"AAPL", "Apple Inc.", 1, 1, 10000};
        instruments.addInstrument(aapl);
    }
};

TEST_F(IntegrationTest, FullTradingSession)
{
    // 1. Initial State: Empty
    EXPECT_TRUE(book.bestBid() == std::nullopt);
    EXPECT_TRUE(book.bestAsk() == std::nullopt);

    // 2. Add Liquidity (Sell Orders)
    // Sell 100 @ 150.00 (1500000)
    Order sell1{1, Side::Sell, OrderType::Limit, 1500000, 100, 100, 1000};
    auto trades1 = book.addOrder(sell1);
    EXPECT_TRUE(trades1.empty());

    // Sell 50 @ 150.05 (1500500)
    Order sell2{2, Side::Sell, OrderType::Limit, 1500500, 50, 50, 1001};
    book.addOrder(sell2);

    // Verify Ask Depth
    auto asks = book.askDepth(5);
    ASSERT_EQ(2, asks.size());
    EXPECT_EQ(1500000, asks[0].price);
    EXPECT_EQ(100, asks[0].totalQty);
    EXPECT_EQ(1500500, asks[1].price);

    // 3. Add Liquidity (Buy Orders)
    // Buy 200 @ 149.90 (1499000)
    Order buy1{3, Side::Buy, OrderType::Limit, 1499000, 200, 200, 2000};
    book.addOrder(buy1);

    // Verify Spread
    // Best Ask: 150.00, Best Bid: 149.90
    auto spread = book.spread();
    EXPECT_EQ(1000, spread.value()); // 1500000 - 1499000 = 1000 (0.10)

    // 4. Crossing Order (Trade)
    // Buy 120 @ 150.00 (Aggressive, match against sell1)
    // Should fill 100 @ 150.00 (clearing sell1)
    // Remaining 20 should post @ 150.00? No, limit was 150.00, so it matches.
    // If it matches partial, remainder stays at limit price.
    // Wait, logic: match order. If remainder > 0 and limit, insert.
    // Price 150.00.
    // Matches sell1 (150.00). Fill 100.
    // Remainder 20. Next ask is 150.05. 150.00 < 150.05. No match.
    // Insert Buy 20 @ 150.00.

    Order aggressiveBuy{4, Side::Buy, OrderType::Limit, 1500000, 120, 120, 3000};
    auto trades2 = book.addOrder(aggressiveBuy);

    // Verify Trades
    ASSERT_EQ(1, trades2.size());
    EXPECT_EQ(100, trades2[0].qty);
    EXPECT_EQ(1500000, trades2[0].price);
    EXPECT_EQ(1, trades2[0].makerOrderId); // Sell1
    EXPECT_EQ(4, trades2[0].takerOrderId);

    // Verify Book State
    // Sell side: Sell1 filled/removed. Sell2 remains.
    // Buy side: Buy1 (149.90). AggressiveBuy remainder (20 @ 150.00).
    // New Best Bid: 150.00. New Best Ask: 150.05.

    EXPECT_EQ(1500500, book.bestAsk().value());
    EXPECT_EQ(1500000, book.bestBid().value());
    EXPECT_EQ(500, book.spread().value()); // 0.05 spread

    // 5. Cancel Remainder
    bool cancelled = book.cancelOrder(4);
    EXPECT_TRUE(cancelled);

    // Verify Bid reverted to 149.90
    EXPECT_EQ(1499000, book.bestBid().value());
}
