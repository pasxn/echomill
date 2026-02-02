#pragma once

#include "order.hpp"
#include "trade.hpp"

#include <list>
#include <vector>

namespace echomill {

class PriceLevel {
public:
    explicit PriceLevel(Price price);

    // Getters
    [[nodiscard]] Price price() const { return m_price; }
    [[nodiscard]] Qty totalQty() const { return m_totalQty; }
    [[nodiscard]] int orderCount() const { return static_cast<int>(m_orders.size()); }
    [[nodiscard]] bool empty() const { return m_orders.empty(); }

    // Add order to back of queue (time priority)
    void addOrder(Order order);

    // Remove specific order by ID
    bool removeOrder(OrderId id);

    // Reduce quantity of specific order (for cancel partial)
    bool reduceOrder(OrderId id, Qty reduceBy);

    // Match against this level (returns trades, modifies orders)
    // Fills orders front-to-back (FIFO)
    std::vector<Trade> match(Order& aggressiveOrder, Timestamp execTime);

    // Get front order (for inspection)
    [[nodiscard]] const Order& front() const { return m_orders.front(); }

    // Iterator access (for depth queries)
    [[nodiscard]] auto begin() const { return m_orders.begin(); }
    [[nodiscard]] auto end() const { return m_orders.end(); }

private:
    Price m_price;
    Qty m_totalQty;
    std::list<Order> m_orders; // FIFO queue (list for stable iterators)
};

} // namespace echomill
