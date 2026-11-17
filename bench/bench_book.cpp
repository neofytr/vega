// Random add / cancel / aggressive-execute stream around a drifting mid; reports ns per event.
#include <chrono>
#include <cstdio>
#include <random>
#include <vector>

#include "vega/order_book.hpp"

using namespace vega;

int main() {
    constexpr uint32_t kEvents = 5'000'000, kIds = kEvents;
    OrderBook book(0, 20'000, kIds);
    std::mt19937_64 rng(42);
    std::vector<uint32_t> live;
    live.reserve(kIds);
    int32_t mid = 10'000;
    uint32_t next_id = 0;

    const auto t0 = std::chrono::steady_clock::now();
    for (uint32_t e = 0; e < kEvents; ++e) {
        const uint64_t r = rng();
        const int kind = static_cast<int>(r % 100);
        if (kind < 55 || live.empty()) {                     // add near the mid
            const bool buy = (r >> 8) & 1;
            const int32_t off = 1 + static_cast<int32_t>((r >> 9) % 20);
            book.add(next_id, buy ? Side::Buy : Side::Sell, buy ? mid - off : mid + off, 1 + (r >> 16) % 100);
            live.push_back(next_id++);
        } else if (kind < 90) {                              // cancel a random live order
            const size_t k = (r >> 8) % live.size();
            book.cancel(live[k]);
            live[k] = live.back();
            live.pop_back();
        } else {                                             // marketable order
            book.execute(((r >> 8) & 1) ? Side::Buy : Side::Sell, 1 + (r >> 16) % 150);
        }
        if ((e & 0xFFFF) == 0) mid += static_cast<int32_t>((r >> 40) % 5) - 2;
    }
    const double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count();
    std::printf("%u events, %.1f ns/event\n", kEvents, ns / kEvents);
}
