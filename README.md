# Vega

Market microstructure toolkit in C++17: a price-time-priority limit order book,
an SPSC ring buffer for feed-to-strategy hand-off, and (in progress) an
event-driven backtester with queue-position-aware fills for market-making research.

> Status: work in progress. The order book, ring buffer, tests and benchmark work.
> Backtester and market-making study are being built now.

## Order book (`include/vega/order_book.hpp`)

- Prices are integer ticks in a fixed band; each level is an intrusive doubly linked
  FIFO through a preallocated order pool, so add, cancel and fill-at-front are O(1)
  with no allocation on the hot path.
- Best bid/ask pointers move only when a level empties.
- `queue_ahead(id)`: quantity resting ahead of an order at its level, the input a
  queue-position-aware fill model needs.

Benchmark (`bench/bench_book.cpp`, 5M random add/cancel/execute events, -O3):

```
5000000 events, 29.3 ns/event
```

## SPSC ring (`include/vega/spsc_ring.hpp`)

Single producer, single consumer, acquire/release only (no CAS); head and tail on
separate cache lines, each side caches the other's index to avoid needless
cross-core loads. Tested with 1M items across two threads.

## Build

```bash
cmake -S . -B build && cmake --build build -j
./build/test_book && ./build/bench_book
```

## Roadmap

- [x] Order book with O(1) levels, time priority, queue position
- [x] SPSC ring buffer, cross-thread test
- [ ] Message parser and replay of recorded tick data through the ring
- [ ] Event-driven backtester: same strategy code as live, queue-position fills, latency injection, fees
- [ ] Market making: Avellaneda-Stoikov with inventory skew vs a PPO agent
- [ ] Order-flow-imbalance signal over the top 5 levels
