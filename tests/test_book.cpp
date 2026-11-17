#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <thread>

#include "vega/order_book.hpp"
#include "vega/spsc_ring.hpp"

using namespace vega;

static void book_basics() {
    OrderBook b(1000, 200, 100);
    b.add(1, Side::Buy, 1050, 10);
    b.add(2, Side::Buy, 1050, 5);   // behind order 1 at the same price
    b.add(3, Side::Buy, 1049, 7);
    b.add(4, Side::Sell, 1052, 8);
    assert(b.best_bid() == 1050 && b.best_ask() == 1052);
    assert(b.depth(Side::Buy, 1050) == 15 && b.queue_ahead(2) == 10);

    auto f = b.execute(Side::Sell, 12);       // takes all of 1, then 2 of order 2 (time priority)
    assert(f.size() == 2 && f[0].maker_id == 1 && f[0].qty == 10 && f[1].maker_id == 2 && f[1].qty == 2);
    assert(b.depth(Side::Buy, 1050) == 3 && b.queue_ahead(2) == 0);

    b.cancel(2);                               // level empties, best bid moves down
    assert(b.best_bid() == 1049);
    b.cancel(2);                               // cancelling twice is a no-op
    f = b.execute(Side::Buy, 100);             // sweeps the only ask
    assert(f.size() == 1 && b.best_ask() == OrderBook::kNone);
}

static void ring_crosses_threads() {
    static SpscRing<uint64_t, 1024> q;
    constexpr uint64_t n = 1'000'000;
    std::thread prod([] { for (uint64_t i = 0; i < n;) if (q.push(i)) ++i; });
    uint64_t expect = 0;
    while (expect < n) if (auto v = q.pop()) { assert(*v == expect); ++expect; }
    prod.join();
}

int main() {
    book_basics();
    ring_crosses_threads();
    std::puts("vega tests: ok");
}
