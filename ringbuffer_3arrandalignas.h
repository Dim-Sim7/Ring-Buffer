#pragma once
#include <cstddef>

/**
 So in this version we aligned writeIdx and readIdx to their
 own cache lines so that they aren't shared on one single cache line

 This helpes reduce false sharing, which in turn, doesnt invalidate the other index
 on another core when an RFO fires.

 I also removed the use of vector and just implemented an array.
 I can align the start of this array on a new cache line and I dont have to follow
 the pointers to wherever the heap allocator placed the buffer on each data access.
 This will lead to faster memory access

 The tradeoff is the array is on the stack and must be known at compile time

 On my CPU (AMD Ryzen 5 7600x 6-Core Processor) after fixing false sharing there was not much
 difference as the cost of false sharing is relatively cheap because the cores are so close together
 L3 has 1 instance shared among all 6 cores. Splitting the indices (read, write) onto seperate cache
 lines doubles the number of lines being transferred per iteration, and on a tightly couple
 single CCX chip that overhead outweighs the benefit of eliminating the false sharing invalidations
 In practice, using alignas is the correct thing to do
2.41659e+07 ops/sec

 Performance counter stats for './bench3':

        4435089366      cycles:u                                                              
         527362866      instructions:u                                                        
          57742029      cache-references:u     x3 the amount of cache references from before                                      
          55690810      cache-misses:u                                                        

       0.415711468 seconds time elapsed

       0.828056000 seconds user
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
            std::cout << item << ' ';
        }

        std::cout << "\nreadIdx: " << readIdx_.load()
                << " writeIdx: " << writeIdx_.load()
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
