#pragma once
#include <atomic>
#include <cstddef>
#include <thread>
/**
    Introduce memory ordering (explain, find resource)
        https://learn.arm.com/learning-paths/servers-and-cloud-computing/arm-cpp-memory-model/2/
        https://stackoverflow.com/questions/12346487/what-do-each-memory-order-mean
    Iterations = 10 000 000
    
    2.13448e+08 ops/sec
 Performance counter stats for './bench4':

         491495246      cycles:u                                                              
         304619111      instructions:u                                                        
           9046111      cache-references:u          9 million                                         
           6515955      cache-misses:u              6.5 million. Still a lot of misses > 2/3rds                                              

       0.047936385 seconds time elapsed

       0.093739000 seconds user
       0.000000000 seconds sys

    In this iteration we implement memory ordering which allow you to specify how memory accesses,
    including regular, non-atomic accesses are ordered among atomic operations.

    Each memory access has their own restrictions and allows the compiler to perform optimisations
    based on which access method is being used and where

    std::memory_order_relaxed is the weakest as it allows for much less synchronisation by removing
    happens-before restrictions and can have optimisations performed on them. It is the cheapest possible
    atomic operation

    A release operation preventes ordinary loads and stored from being reordered after the
    atomic operation, whereas an acquire operation prevents loads and stores from being reordered
    before the atomic operation.  Everything else can still be moved around
    The combination of preventing stores being moved after and loads being moved before the respective
    atomic operation makes sure that whatever the acquiring thread gets to see is consistent

    Previously I used std::memory_order_seq_cst, which enforces global 
    sequential ordering and restricts compiler/CPU reordering much more aggressively.

    In my SPSC queue this introduced unnecessary synchronization overhead and reduced 
    optimization opportunities.

    Switching to acquire/release semantics reduced ordering constraints and improved 
    cache/coherence behavior and overall performance.

    Cache coherency ensures cached copies of memory is consistent across CPU cores
    and that they arent stale

*/
template<typename T, size_t Capacity>
class RingBuffer {
public:

    bool push(const T& item);
    bool pop(T& item);
    void debug() const {
        std::cout << "Buffer: ";

        for (const auto& item : data_) {
            std::cout << item << '\n';
        }

        std::cout << "\nreadIdx: " << readIdx_.load(std::memory_order_relaxed)
                << " writeIdx: " << writeIdx_.load(std::memory_order_relaxed)
                << "\n\n";
    }
    static void print_layout() {
        std::cout << "offsetof writeIdx: " << offsetof(RingBuffer, writeIdx_) << "\n";
        std::cout << "offsetof readIdx:  " << offsetof(RingBuffer, readIdx_)  << "\n";
        std::cout << "offsetof data:     " << offsetof(RingBuffer, data_)     << "\n";
    }
private:
    alignas(64) std::atomic<size_t> writeIdx_{0};
    alignas(64) std::atomic<size_t> readIdx_{0};
    alignas(64) T data_[Capacity];
};

template<typename T, size_t Capacity>
bool RingBuffer<T, Capacity>::push(const T& item) {

    size_t write = writeIdx_.load(std::memory_order_relaxed);
    size_t nextWrite = (write + 1) % Capacity;

    // full -- before, I was modifying readIdx here which caused data race
    // one item is left unused to indicate the queue is full, when writeIdx_ is one item
    // behind readIdx_
    if (nextWrite == readIdx_.load(std::memory_order_acquire)) {
        return false;
    }

    data_[write] = item;

    writeIdx_.store(nextWrite, std::memory_order_release);

    return true;
}

template<typename T, size_t Capacity>
bool RingBuffer<T, Capacity>::pop(T& item) {

    size_t readIdx = readIdx_.load(std::memory_order_relaxed);

    // empty
    if (readIdx == writeIdx_.load(std::memory_order_acquire)) {
        return false;
    }

    item = data_[readIdx];

    readIdx_.store((readIdx + 1) % Capacity, std::memory_order_release);

    return true;
}
