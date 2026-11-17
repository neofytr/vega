#pragma once
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace vega {

enum class Side : uint8_t { Buy, Sell };

// Price-time priority limit order book for one instrument.
//
// Prices are integer ticks inside a fixed band [base, base + levels). Each price
// level is an intrusive doubly linked FIFO through the order pool, so add, cancel
// and fill-at-front are O(1) with no allocation on the hot path. Order ids index
// straight into the pool. ponytail: fixed price band and dense ids; re-centre the
// band / hash ids if an instrument moves far or ids are sparse.
class OrderBook {
public:
    static constexpr int32_t kNone = -1;

    struct Fill { uint32_t maker_id; int64_t qty; int32_t price; };

    OrderBook(int32_t base_tick, int32_t levels, uint32_t max_orders)
        : base_(base_tick), levels_(levels), bid_(levels), ask_(levels), pool_(max_orders) {}

    void add(uint32_t id, Side side, int32_t price, int64_t qty) {
        const int32_t i = price - base_;
        if (i < 0 || i >= levels_) throw std::out_of_range("price outside band");
        Order& o = pool_.at(id);
        o = Order{qty, price, kNone, kNone, side, true};
        Level& L = (side == Side::Buy ? bid_ : ask_)[i];
        o.prev = L.tail;
        if (L.tail != kNone) pool_[L.tail].next = static_cast<int32_t>(id); else L.head = static_cast<int32_t>(id);
        L.tail = static_cast<int32_t>(id);
        L.qty += qty;
        if (side == Side::Buy) { if (best_bid_ == kNone || i > best_bid_) best_bid_ = i; }
        else                   { if (best_ask_ == kNone || i < best_ask_) best_ask_ = i; }
    }

    void cancel(uint32_t id) {
        Order& o = pool_.at(id);
        if (!o.live) return;
        unlink(id, o, o.qty);
    }

    // Aggressive order of `qty` against the opposite side; returns the fills.
    // Walks levels best-first and each level front-to-back (time priority).
    std::vector<Fill> execute(Side aggressor, int64_t qty) {
        std::vector<Fill> fills;
        std::vector<Level>& book = aggressor == Side::Buy ? ask_ : bid_;
        int32_t& best = aggressor == Side::Buy ? best_ask_ : best_bid_;
        while (qty > 0 && best != kNone) {
            const int32_t id = book[best].head;
            Order& o = pool_[id];
            const int64_t take = o.qty < qty ? o.qty : qty;
            fills.push_back({static_cast<uint32_t>(id), take, o.price});
            qty -= take;
            if (take == o.qty) unlink(static_cast<uint32_t>(id), o, take);
            else { o.qty -= take; book[best].qty -= take; }
        }
        return fills;
    }

    int32_t best_bid() const { return best_bid_ == kNone ? kNone : best_bid_ + base_; }
    int32_t best_ask() const { return best_ask_ == kNone ? kNone : best_ask_ + base_; }
    int64_t depth(Side s, int32_t price) const { return (s == Side::Buy ? bid_ : ask_)[price - base_].qty; }

    // Quantity resting ahead of order `id` at its level: what a queue-position-aware
    // backtester needs to decide whether a resting order would have filled.
    int64_t queue_ahead(uint32_t id) const {
        int64_t ahead = 0;
        for (int32_t j = pool_.at(id).prev; j != kNone; j = pool_[j].prev) ahead += pool_[j].qty;
        return ahead;
    }

private:
    struct Order { int64_t qty; int32_t price; int32_t prev, next; Side side; bool live; };
    struct Level { int32_t head = kNone, tail = kNone; int64_t qty = 0; };

    void unlink(uint32_t id, Order& o, int64_t qty) {
        const int32_t i = o.price - base_;
        Level& L = (o.side == Side::Buy ? bid_ : ask_)[i];
        if (o.prev != kNone) pool_[o.prev].next = o.next; else L.head = o.next;
        if (o.next != kNone) pool_[o.next].prev = o.prev; else L.tail = o.prev;
        L.qty -= qty;
        o.live = false;
        (void)id;
        if (L.head == kNone) {  // level emptied: move best pointer to the next non-empty level
            if (o.side == Side::Buy && i == best_bid_) {
                while (best_bid_ >= 0 && bid_[best_bid_].head == kNone) --best_bid_;
                if (best_bid_ < 0) best_bid_ = kNone;
            } else if (o.side == Side::Sell && i == best_ask_) {
                while (best_ask_ < levels_ && ask_[best_ask_].head == kNone) ++best_ask_;
                if (best_ask_ >= levels_) best_ask_ = kNone;
            }
        }
    }

    int32_t base_, levels_;
    std::vector<Level> bid_, ask_;
    std::vector<Order> pool_;
    int32_t best_bid_ = kNone, best_ask_ = kNone;
};

}  // namespace vega
