#pragma once
#include <atomic>
#include <cstddef>
#include <thread>
#include <cstring>
/**

try implement what is in here
https://abhinavag.medium.com/a-fast-circular-ring-buffer-4d102ef4d4a3

simulate producing and consuming real market data objects, no one cares if im just doing integers

*/
template<typename T, size_t Capacity>
class RingBuffer {
public:

    bool push(const T& item);
    bool pop(T& item);
    bool pop_bulk(T* dst, size_t count);
    bool push_bulk(const T* src, size_t count);
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
    alignas(64) std::atomic<size_t> readIdx_{0};
    alignas(64) size_t writeIdxCached_{0};
    alignas(64) std::atomic<size_t> writeIdx_{0};
    alignas(64) size_t readIdxCached_{0};
    alignas(64) T data_[Capacity];
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

template<typename T, size_t Capacity>
bool RingBuffer<T, Capacity>::push_bulk(const T* src, size_t count) {

    static_assert(std::is_trivially_copyable_v<T>);

    size_t write = writeIdx_.load(std::memory_order_relaxed);

    size_t available;

    if (readIdxCached_ <= write) {
        available = Capacity - (write - readIdxCached_) - 1;
    } else {
        available = readIdxCached_ - write - 1;
    }

    if (count > available) {
        readIdxCached_ = readIdx_.load(std::memory_order_acquire);

        if (readIdxCached_ <= write) {
            available = Capacity - (write - readIdxCached_) - 1;
        } else {
            available = readIdxCached_ - write - 1;
        }

        if (count > available) {
            return false;
        }
    }

    size_t firstPart = std::min(count, Capacity - write);

    std::memcpy(
        &data_[write],
        src,
        firstPart * sizeof(T)
    );

    std::memcpy(
        &data_[0],
        src + firstPart,
        (count - firstPart) * sizeof(T)
    );

    size_t nextWrite = (write + count) % Capacity;

    writeIdx_.store(nextWrite, std::memory_order_release);

    return true;
}

template<typename T, size_t Capacity>
bool RingBuffer<T, Capacity>::pop_bulk(T* dst, size_t count) {

    static_assert(std::is_trivially_copyable_v<T>);

    size_t read = readIdx_.load(std::memory_order_relaxed);

    size_t available;

    if (writeIdxCached_ >= read) {
        available = writeIdxCached_ - read;
    } else {
        available = Capacity - (read - writeIdxCached_);
    }

    if (count > available) {
        writeIdxCached_ = writeIdx_.load(std::memory_order_acquire);

        if (writeIdxCached_ >= read) {
            available = writeIdxCached_ - read;
        } else {
            available = Capacity - (read - writeIdxCached_);
        }

        if (count > available) {
            return false;
        }
    }

    size_t firstPart = std::min(count, Capacity - read);

    std::memcpy(
        dst,
        &data_[read],
        firstPart * sizeof(T)
    );

    std::memcpy(
        dst + firstPart,
        &data_[0],
        (count - firstPart) * sizeof(T)
    );

    size_t nextRead = (read + count) % Capacity;

    readIdx_.store(nextRead, std::memory_order_release);

    return true;
}


