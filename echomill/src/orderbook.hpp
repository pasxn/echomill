#pragma once

#include "booklevel.hpp"
#include "order.hpp"
#include "pricelevel.hpp"
#include "trade.hpp"

#include <functional>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace echomill {

class OrderBook {
public:
    // Callback type for trade notifications
    using TradeCallback = std::function<void(const Trade&)>;

    OrderBook() = default;

    // Set optional trade callback (called for each trade)
    void setTradeCallback(TradeCallback callback);

    // Main operations
    std::vector<Trade> addOrder(Order order);
    bool cancelOrder(OrderId id);
    bool modifyOrder(OrderId id, Qty newQty); // Reduce qty only

    // Queries
    [[nodiscard]] std::optional<Price> bestBid() const;
    [[nodiscard]] std::optional<Price> bestAsk() const;
    [[nodiscard]] std::optional<Price> spread() const;

    // Get depth (top N levels per side)
    [[nodiscard]] std::vector<BookLevel> bidDepth(size_t levels) const;
    [[nodiscard]] std::vector<BookLevel> askDepth(size_t levels) const;

    // Order lookup
    [[nodiscard]] const Order& findOrder(OrderId id) const;

    // Statistics
    [[nodiscard]] size_t bidLevelCount() const { return m_bids.size(); }
    [[nodiscard]] size_t askLevelCount() const { return m_asks.size(); }
    [[nodiscard]] size_t orderCount() const { return m_orderIndex.size(); }

private:
    // Check if order can cross the book
    [[nodiscard]] bool canMatch(const Order& order) const;

    // Match aggressive order against opposite side
    std::vector<Trade> matchOrder(Order& order);

protected:
    // Insert passive order into book
    void insertOrder(const Order& order);

private:
    // Remove empty price levels
    void cleanupLevel(Side side, Price price);

    // Get current timestamp
    [[nodiscard]] static Timestamp now();

protected:
    // Bids: sorted descending (highest price first)
    std::map<Price, PriceLevel, std::greater<Price>> m_bids;

    // Asks: sorted ascending (lowest price first)
    std::map<Price, PriceLevel, std::less<Price>> m_asks;

    // Fast order lookup by ID: maps to (Side, Price)
    std::unordered_map<OrderId, std::pair<Side, Price>> m_orderIndex;

    // Optional trade callback
    TradeCallback m_tradeCallback;
};

} // namespace echomill
