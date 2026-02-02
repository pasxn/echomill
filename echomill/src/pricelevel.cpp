#include "pricelevel.hpp"

#include <algorithm>

namespace echomill {

PriceLevel::PriceLevel(Price price) : m_price(price), m_totalQty(0) {}

void PriceLevel::addOrder(Order order)
{
    m_totalQty += order.remaining;
    m_orders.push_back(std::move(order));
}

bool PriceLevel::removeOrder(OrderId id)
{
    auto iterator = std::find_if(m_orders.begin(), m_orders.end(), [id](const Order& order) { return order.id == id; });

    if (iterator == m_orders.end()) {
        return false;
    }

    m_totalQty -= iterator->remaining;
    m_orders.erase(iterator);
    return true;
}

bool PriceLevel::reduceOrder(OrderId id, Qty reduceBy)
{
    auto iterator = std::find_if(m_orders.begin(), m_orders.end(), [id](const Order& order) { return order.id == id; });

    if (iterator == m_orders.end()) {
        return false;
    }

    if (reduceBy >= iterator->remaining) {
        // Full cancellation
        m_totalQty -= iterator->remaining;
        m_orders.erase(iterator);
    } else {
        // Partial cancellation
        iterator->remaining -= reduceBy;
        m_totalQty -= reduceBy;
    }
    return true;
}

std::vector<Trade> PriceLevel::match(Order& aggressiveOrder, Timestamp execTime)
{
    std::vector<Trade> trades;

    while (!m_orders.empty() && aggressiveOrder.remaining > 0) {
        Order& passiveOrder = m_orders.front();

        Qty fillQty = std::min(aggressiveOrder.remaining, passiveOrder.remaining);

        Trade trade{};
        trade.takerOrderId = aggressiveOrder.id;
        trade.makerOrderId = passiveOrder.id;
        trade.takerSide = aggressiveOrder.side;
        trade.price = m_price;
        trade.qty = fillQty;
        trade.timestamp = execTime;
        trades.push_back(trade);

        aggressiveOrder.fill(fillQty);
        passiveOrder.fill(fillQty);
        m_totalQty -= fillQty;

        if (passiveOrder.isFilled()) {
            m_orders.pop_front();
        }
    }

    return trades;
}

} // namespace echomill
