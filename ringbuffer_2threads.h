#pragma once
#include <cstddef>
#include <vector>

/**
    Ring Buffer: Threads

    now we have the naive working we see that in the benchmark
    a data race occurs
    
    The consumer may observe writeIdx_ update before data_
    becomes visible

    There is no synchronisation

    Both cores may cache values independantly
    compiler may reorder operations
    cpu may reorder visibility
    writes may not become visible immediately
    both threads are accessing shared state without synchronization guarantees.
    the consumer may observe the index update before the actual data write becomes visible.

    The FIX is to introduce atomics
    3.49876e+07 ops/sec

    Performance counter stats for './bench':

            24776530      cache-references:u                                                    
            24500044      cache-misses:u          98% of all cache refs are misses!!!                                            

        0.287436306 seconds time elapsed

        0.578305000 seconds user
        0.000000000 seconds sys
    
        // 35 852 599 roughly 3x operations per read-write pair
        Why?
    So now I've fixed the data races but now im getting only cache misses

    A cache miss is defined as a failed attempt to read or write data in the L1 cache,
    which results in slower memory accesses such as L2, L3 or RAM

    With default atomic load and store operations we get sequentially consistent ordering,
    the strictest memory ordering available, which guarantees all threads observe all 
    atomic operations in the same global order.


    Core A push:
    shared line load  → 1 ref, MISS (coherence)
    data_[write]      → 1 ref, likely MISS (cold/capacity)
    writeIdx.store()  → RFO on the same shared line, not a new reference

    Core B pop:
    shared line load  → 1 ref, MISS (coherence, line invalidated by Core A's RFO)
    data_[read]       → 1 ref
    readIdx.store()   → RFO on the same shared line, fires invalidation back at Core A

    https://en.wikipedia.org/wiki/MESI_protocol
    When a core wants to write to a cache line it needs exclusive ownership first. 
    It broadcasts an RFO across the interconnect, which invalidates all other copies before 
    granting the requester Modified state. There are two cases depending on the current 
    state of the line:

    Line is Invalid: the requesting core has no data at all and must fetch it from L3 or 
    from the owning core, then broadcast an invalidation. 
    The requester gets the line in Modified state.

    Line is Shared: the requesting core already holds a valid read copy and just needs to 
    upgrade to write permission. It broadcasts an invalidation to all other cores 
    holding the line, they transition to Invalid, and the requester upgrades to 
    Modified state without needing to fetch.
    
    Line is Modified: on another core the protocol first asks that core to relinquish ownership. 
    That core either writes the line back to L3 or forwards it directly to the requester, 
    then transitions to Invalid. Only then does the requester get Modified ownership. 
    This requires an extra round trip and is the most expensive case.

    False Sharing:

    When two threads share a cache line containing both indices, 
    every store to either index fires an RFO that invalidates the entire line on the other core.
    When two threads share a cache line containing both indices, 
    every store to either index fires an RFO that invalidates the entire line on the other core. 
    So the producer writing writeIdx silently evicts the consumer's copy of readIdx too, and vice versa. 
    The line spends most of its time Invalid
*/
template<typename T, size_t Capacity>
class RingBuffer {
public:
    RingBuffer() : data_(Capacity, 0) {}
    
    bool push(const T& item);
    bool pop(T& item);
    void debug() const {
        std::cout << "Buffer: ";

        for (const auto& item : data_) {
            std::cout << item << ' ';
        }

        std::cout << "\nreadIdx: " << readIdx_.load()
                << " writeIdx: " << writeIdx_.load()
                << "\n\n";
    }
private:
    std::atomic<size_t> writeIdx_{0};
    std::atomic<size_t> readIdx_{0};
    std::vector<T> data_;
};

template<typename T, size_t Capacity>
bool RingBuffer<T, Capacity>::push(const T& item) {

    size_t write = writeIdx_.load();
    size_t nextWrite = (write + 1) % Capacity;

    // full -- before, I was modifying readIdx here which caused data race
    if (nextWrite == readIdx_.load()) {
        return false;
    }

    data_[write] = item;

    writeIdx_.store(nextWrite);

    return true;
}

template<typename T, size_t Capacity>
bool RingBuffer<T, Capacity>::pop(T& item) {

    size_t read = readIdx_.load();

    // empty
    if (read == writeIdx_.load()) {
        return false;
    }

    item = data_[read];

    readIdx_.store((read + 1) % Capacity);

    return true;
}
