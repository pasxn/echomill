#include "orderbook.hpp"

#include <chrono>
#include <stdexcept>
#include <string>

namespace echomill {

void OrderBook::setTradeCallback(TradeCallback callback) { m_tradeCallback = std::move(callback); }

std::vector<Trade> OrderBook::addOrder(Order order)
{
    std::vector<Trade> trades;

    // Try to match if order can cross the book
    if (canMatch(order)) {
        trades = matchOrder(order);

        // Notify via callback
        if (m_tradeCallback != nullptr) {
            for (const auto& trade : trades) {
                m_tradeCallback(trade);
            }
        }
    }

    // Insert remaining quantity as passive order (limit orders only)
    if (!order.isFilled() && order.type == OrderType::Limit) {
        insertOrder(order);
    }

    return trades;
}

bool OrderBook::cancelOrder(OrderId id)
{
    auto indexIterator = m_orderIndex.find(id);
    if (indexIterator == m_orderIndex.end()) {
        return false;
    }

    auto [side, price] = indexIterator->second;
    bool removed = false;

    if (side == Side::Buy) {
        auto levelIterator = m_bids.find(price);
        if (levelIterator != m_bids.end()) {
            removed = levelIterator->second.removeOrder(id);
            if (levelIterator->second.empty()) {
                m_bids.erase(levelIterator);
            }
        }
    } else {
        auto levelIterator = m_asks.find(price);
        if (levelIterator != m_asks.end()) {
            removed = levelIterator->second.removeOrder(id);
            if (levelIterator->second.empty()) {
                m_asks.erase(levelIterator);
            }
        }
    }

    if (removed) {
        m_orderIndex.erase(indexIterator);
    }
    return removed;
}

bool OrderBook::modifyOrder(OrderId id, Qty newQty)
{
    auto indexIterator = m_orderIndex.find(id);
    if (indexIterator == m_orderIndex.end()) {
        return false;
    }

    auto [side, price] = indexIterator->second;

    if (side == Side::Buy) {
        auto levelIterator = m_bids.find(price);
        if (levelIterator != m_bids.end()) {
            // Find current remaining qty to calculate reduction
            for (const auto& order : levelIterator->second) {
                if (order.id == id) {
                    if (newQty >= order.remaining) {
                        return false; // Can only reduce
                    }
                    Qty reduceBy = order.remaining - newQty;
                    if (newQty == 0) {
                        return cancelOrder(id);
                    }
                    return levelIterator->second.reduceOrder(id, reduceBy);
                }
            }
        }
    } else {
        auto levelIterator = m_asks.find(price);
        if (levelIterator != m_asks.end()) {
            for (const auto& order : levelIterator->second) {
                if (order.id == id) {
                    if (newQty >= order.remaining) {
                        return false;
                    }
                    Qty reduceBy = order.remaining - newQty;
                    if (newQty == 0) {
                        return cancelOrder(id);
                    }
                    return levelIterator->second.reduceOrder(id, reduceBy);
                }
            }
        }
    }

    return false;
}

std::optional<Price> OrderBook::bestBid() const
{
    if (m_bids.empty()) {
        return std::nullopt;
    }
    return m_bids.begin()->first;
}

std::optional<Price> OrderBook::bestAsk() const
{
    if (m_asks.empty()) {
        return std::nullopt;
    }
    return m_asks.begin()->first;
}

std::optional<Price> OrderBook::spread() const
{
    auto bid = bestBid();
    auto ask = bestAsk();
    if (!bid.has_value() || !ask.has_value()) {
        return std::nullopt;
    }
    return ask.value() - bid.value();
}

std::vector<BookLevel> OrderBook::bidDepth(size_t levels) const
{
    std::vector<BookLevel> result;
    result.reserve(levels);

    size_t count = 0;
    for (const auto& [price, level] : m_bids) {
        if (count >= levels) {
            break;
        }
        result.push_back({price, level.totalQty(), level.orderCount()});
        ++count;
    }

    return result;
}

std::vector<BookLevel> OrderBook::askDepth(size_t levels) const
{
    std::vector<BookLevel> result;
    result.reserve(levels);

    size_t count = 0;
    for (const auto& [price, level] : m_asks) {
        if (count >= levels) {
            break;
        }
        result.push_back({price, level.totalQty(), level.orderCount()});
        ++count;
    }

    return result;
}

const Order& OrderBook::findOrder(OrderId id) const
{
    auto indexIterator = m_orderIndex.find(id);
    if (indexIterator == m_orderIndex.end()) {
        throw std::out_of_range("Order not found: " + std::to_string(id));
    }

    auto [side, price] = indexIterator->second;

    if (side == Side::Buy) {
        auto levelIterator = m_bids.find(price);
        if (levelIterator != m_bids.end()) {
            for (const auto& order : levelIterator->second) {
                if (order.id == id) {
                    return order;
                }
            }
        }
    } else {
        auto levelIterator = m_asks.find(price);
        if (levelIterator != m_asks.end()) {
            for (const auto& order : levelIterator->second) {
                if (order.id == id) {
                    return order;
                }
            }
        }
    }

    throw std::out_of_range("Order index is inconsistent for ID: " + std::to_string(id));
}

bool OrderBook::canMatch(const Order& order) const
{
    if (order.type == OrderType::Market) {
        // Market orders always try to match
        if (order.side == Side::Buy) {
            return !m_asks.empty();
        }
        return !m_bids.empty();
    }

    // Limit order: check if price crosses
    if (order.side == Side::Buy) {
        if (m_asks.empty()) {
            return false;
        }
        return order.price >= m_asks.begin()->first;
    }

    if (m_bids.empty()) {
        return false;
    }
    return order.price <= m_bids.begin()->first;
}

std::vector<Trade> OrderBook::matchOrder(Order& order)
{
    std::vector<Trade> allTrades;
    Timestamp execTime = now();

    if (order.side == Side::Buy) {
        // Match against asks (lowest first)
        while (order.remaining > 0 && !m_asks.empty()) {
            auto askIterator = m_asks.begin();

            // Check if price is acceptable for limit orders
            if (order.type == OrderType::Limit && order.price < askIterator->first) {
                break;
            }

            // Match at this level
            auto trades = askIterator->second.match(order, execTime);
            allTrades.insert(allTrades.end(), trades.begin(), trades.end());

            // Remove filled orders from index
            for (const auto& trade : trades) {
                auto it = std::find_if(askIterator->second.begin(), askIterator->second.end(),
                                       [&trade](const Order& o) { return o.id == trade.makerOrderId; });

                if (it == askIterator->second.end() || it->isFilled()) {
                    m_orderIndex.erase(trade.makerOrderId);
                }
            }

            // Remove empty level
            if (askIterator->second.empty()) {
                m_asks.erase(askIterator);
            }
        }
    } else {
        // Match against bids (highest first)
        while (order.remaining > 0 && !m_bids.empty()) {
            auto bidIterator = m_bids.begin();

            // Check if price is acceptable for limit orders
            if (order.type == OrderType::Limit && order.price > bidIterator->first) {
                break;
            }

            // Match at this level
            auto trades = bidIterator->second.match(order, execTime);
            allTrades.insert(allTrades.end(), trades.begin(), trades.end());

            // Remove filled orders from index
            for (const auto& trade : trades) {
                auto it = std::find_if(bidIterator->second.begin(), bidIterator->second.end(),
                                       [&trade](const Order& o) { return o.id == trade.makerOrderId; });

                if (it == bidIterator->second.end() || it->isFilled()) {
                    m_orderIndex.erase(trade.makerOrderId);
                }
            }

            // Remove empty level
            if (bidIterator->second.empty()) {
                m_bids.erase(bidIterator);
            }
        }
    }

    return allTrades;
}

void OrderBook::insertOrder(const Order& order)
{
    if (m_orderIndex.count(order.id)) {
        cancelOrder(order.id);
    }

    m_orderIndex[order.id] = {order.side, order.price};

    if (order.side == Side::Buy) {
        auto [iterator, inserted] = m_bids.try_emplace(order.price, order.price);
        iterator->second.addOrder(order);
    } else {
        auto [iterator, inserted] = m_asks.try_emplace(order.price, order.price);
        iterator->second.addOrder(order);
    }
}

void OrderBook::cleanupLevel(Side side, Price price)
{
    if (side == Side::Buy) {
        auto iterator = m_bids.find(price);
        if (iterator != m_bids.end() && iterator->second.empty()) {
            m_bids.erase(iterator);
        }
    } else {
        auto iterator = m_asks.find(price);
        if (iterator != m_asks.end() && iterator->second.empty()) {
            m_asks.erase(iterator);
        }
    }
}

Timestamp OrderBook::now()
{
    auto timePoint = std::chrono::steady_clock::now();
    auto duration = timePoint.time_since_epoch();
    return static_cast<Timestamp>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
}

} // namespace echomill
