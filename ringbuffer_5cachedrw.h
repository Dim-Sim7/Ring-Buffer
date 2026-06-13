#pragma once
#include <atomic>
#include <cstddef>
#include <thread>
/**
https://rigtorp.se/ringbuffer/
Consider a read operation: the read index needs to be updated and thus that cache line is 
loaded into the L1 cache in exclusive state (see MESI protocol). 
The write index needs to be read in order to check that the queue is not empty 
and is thus loaded into the L1 cache in shared state. Since a queue write operation needs 
to read the read index it causes the reader’s read index cache line to be evicted or 
transition into shared state. Now the read operation requires some cache coherency traffic 
to bring the read index cache line back into exclusive state. In turn a write operation will 
require some cache coherency traffic to bring the write index cache line back into exclusive 
state. In the worst case there will be one cache line transition from shared to exclusive for
 every read and write operation. These cache line state transitions are counted as cache misses. 
 We don’t know the exact implementation details of the cache coherency protocol, but it will behave
 roughly as the MESI protocol.

To reduce the amount of coherency traffic the reader and writer can keep a cached copy of the 
write and read index respectively. In this case when a reader first observes that N items are 
available to read, it caches this information and the N-1 subsequent reads won’t need to read 
the write index. Similarly when a writer first observes that N items are available for writing, 
it caches this information and the N-1 subsequent writes won’t need to read the read index.


4.8801e+08 ops/sec

 Performance counter stats for './bench5':

         217065090      cycles:u                                                              
         274873596      instructions:u                                                        
           3292138      cache-references:u           3.2 mil                                         
           2665055      cache-misses:u               2.6 mil                                  

       0.024545630 seconds time elapsed

       0.040479000 seconds user
       0.000000000 seconds sys


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

    T data_[Capacity];
    alignas(64) std::atomic<size_t> readIdx_{0};
    alignas(64) size_t writeIdxCached_{0};
    alignas(64) std::atomic<size_t> writeIdx_{0};
    alignas(64) size_t readIdxCached_{0};

};

template<typename T, size_t Capacity>
bool RingBuffer<T, Capacity>::push(const T& item) {

    size_t write = writeIdx_.load(std::memory_order_relaxed);
    size_t nextWrite = (write + 1) % Capacity;

    // full -- before, I was modifying readIdx here which caused data race
    // one item is left unused to indicate the queue is full, when writeIdx_ is one item
    // behind readIdx_
    if (nextWrite == readIdxCached_) {
        // updated cached value
        readIdxCached_ = readIdx_.load(std::memory_order_acquire);
        if (nextWrite == readIdxCached_) {
            return false;
        }
    }

    data_[write] = item;

    writeIdx_.store(nextWrite, std::memory_order_release);

    return true;
}

template<typename T, size_t Capacity>
bool RingBuffer<T, Capacity>::pop(T& item) {

    size_t readIdx = readIdx_.load(std::memory_order_relaxed);

    // empty
    if (readIdx == writeIdxCached_) {
        // updated cached value
        writeIdxCached_ = writeIdx_.load(std::memory_order_acquire);
        if (readIdx == writeIdxCached_) {
            return false;
        }
    }

    item = data_[readIdx];

    readIdx_.store((readIdx + 1) % Capacity, std::memory_order_release);

    return true;
}
