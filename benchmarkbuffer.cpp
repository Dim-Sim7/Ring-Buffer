#include <chrono>
#include <iostream>
#include <thread>
#include "ringbuffer_5cachedrw.h"
template<typename Queue>
void bench() {

    constexpr int64_t iterations = 10'000'000;

    Queue q;

    auto consumer = std::thread([&]() {

        for (int i = 0; i < iterations; ++i) {

            int value;

            // wait for items to be pushed
            while (!q.pop(value)) {

            }
            //std::cout << "POPPING VALUES: Value: " << value << " index: " << i << "\n";
            if (value != i) {
                std::cerr << "Value: " << value << " index: " << i << " Data mismatch!\n";
                std::exit(1);
            }
        }
    });
    // pin consumer to core 1
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    pthread_setaffinity_np(consumer.native_handle(), sizeof(cpuset), &cpuset);

    // pin producer (main thread) to core 0
    cpu_set_t cpuset2;
    CPU_ZERO(&cpuset2);
    CPU_SET(0, &cpuset2);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset2), &cpuset2);
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < iterations; ++i) {
        //std::cout << "PUSHING VALUE: Value: " << i << "\n";
        // wait for items to be popped
        while(!q.push(i)) {

        }
    }

    consumer.join();

    auto stop = std::chrono::steady_clock::now();

    auto ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            stop - start
        ).count();

    double opsPerSecond =
        (iterations * 1'000'000'000.0) / ns;

    std::cout << opsPerSecond
              << " ops/sec\n";
}

// perf stat -e cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,dTLB-loads,dTLB-load-misses ./benchX
int main() {
    RingBuffer<int, 1024>::print_layout();
    bench<RingBuffer<int, 1024>>();
}