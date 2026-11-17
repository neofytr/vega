#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <optional>

namespace vega {

// Single-producer single-consumer ring. Only the producer writes head_, only the
// consumer writes tail_, so plain acquire/release loads and stores are enough: no CAS.
// head_ and tail_ live on separate cache lines so the two threads don't false-share.
template <typename T, std::size_t N>
class SpscRing {
    static_assert((N & (N - 1)) == 0, "capacity must be a power of two");

public:
    bool push(const T& v) {
        const auto h = head_.load(std::memory_order_relaxed);
        if (h - tail_cache_ == N) {                       // looks full: refresh our view of tail
            tail_cache_ = tail_.load(std::memory_order_acquire);
            if (h - tail_cache_ == N) return false;
        }
        buf_[h & (N - 1)] = v;
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

    std::optional<T> pop() {
        const auto t = tail_.load(std::memory_order_relaxed);
        if (t == head_cache_) {                           // looks empty: refresh our view of head
            head_cache_ = head_.load(std::memory_order_acquire);
            if (t == head_cache_) return std::nullopt;
        }
        T v = buf_[t & (N - 1)];
        tail_.store(t + 1, std::memory_order_release);
        return v;
    }

private:
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::size_t tail_cache_ = 0;   // producer-private
    alignas(64) std::atomic<std::size_t> tail_{0};
    alignas(64) std::size_t head_cache_ = 0;   // consumer-private
    alignas(64) std::array<T, N> buf_{};
};

}  // namespace vega
