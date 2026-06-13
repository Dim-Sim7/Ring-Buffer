#include <chrono>
#include <iostream>
#include <thread>
#include "ringbuffer_5cachedrw.h"
#include <random>
template<typename Queue>
void bench() {
    constexpr auto max_duration = std::chrono::seconds(15);
    constexpr int  min_burst    = 10;
    constexpr int  max_burst    = 500;
    constexpr auto min_pause    = std::chrono::microseconds(10);
    constexpr auto max_pause    = std::chrono::microseconds(500);
    constexpr auto print_interval = std::chrono::milliseconds(500);

    std::mt19937                       rng(std::random_device{}());
    std::uniform_int_distribution<int> burst_dist(min_burst, max_burst);
    std::uniform_int_distribution<int> pause_dist(
        min_pause.count(), max_pause.count()
    );

    Queue q;
    std::atomic<bool>    done{false};
    std::atomic<int64_t> produced{0};
    std::atomic<int64_t> consumed{0};

    // --- consumer ---
    auto consumer = std::thread([&]() {
        int expected = 0;
        while (true) {
            int value;
            while (!q.pop(value)) {
                if (done.load(std::memory_order_acquire)) return;
            }
            if (value != expected) {
                std::cerr << "Value: " << value
                          << " expected: " << expected
                          << " Data mismatch!\n";
                std::exit(1);
            }
            ++expected;
            consumed.fetch_add(1, std::memory_order_relaxed);
        }
    });

    // --- stats printer ---
    auto stats = std::thread([&]() {
        auto next = std::chrono::steady_clock::now() + print_interval;
        while (!done.load(std::memory_order_acquire)) {
            std::this_thread::sleep_until(next);
            next += print_interval;
            int64_t p = produced.load(std::memory_order_relaxed);
            int64_t c = consumed.load(std::memory_order_relaxed);
            std::cout << "produced: " << std::setw(10) << p
                      << "  consumed: " << std::setw(10) << c
                      << "  queued: "  << std::setw(6)  << (p - c)
                      << "\n";
        }
    });

    // pin consumer to core 1
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    pthread_setaffinity_np(consumer.native_handle(), sizeof(cpuset), &cpuset);

    // pin producer to core 0
    cpu_set_t cpuset2;
    CPU_ZERO(&cpuset2);
    CPU_SET(0, &cpuset2);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset2), &cpuset2);

    auto start = std::chrono::steady_clock::now();

    int64_t i = 0;
    while (true) {
        if (std::chrono::steady_clock::now() - start >= max_duration) break;

        int burst = burst_dist(rng);
        for (int b = 0; b < burst; ++b) {
            while (!q.push(static_cast<int>(i))) {
                if (std::chrono::steady_clock::now() - start >= max_duration)
                    goto done_producing;
            }
            ++i;
            produced.fetch_add(1, std::memory_order_relaxed);
        }

        std::this_thread::sleep_for(
            std::chrono::microseconds(pause_dist(rng))
        );
    }

done_producing:
    done.store(true, std::memory_order_release);

    consumer.join();
    stats.join();

    auto stop = std::chrono::steady_clock::now();
    auto ns   = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    stop - start).count();

    double opsPerSecond = (produced.load() * 1'000'000'000.0) / ns;
    std::cout << "\n--- final ---\n"
              << "Produced:   " << produced.load()  << " items\n"
              << "Consumed:   " << consumed.load()  << " items\n"
              << "Elapsed:    " << ns / 1'000'000    << " ms\n"
              << "Throughput: " << opsPerSecond      << " ops/sec\n";
}
// perf stat -e cache-references,cache-misses ./bench
int main() {
    RingBuffer<int, 1024>::print_layout();
    bench<RingBuffer<int, 1024>>();
}