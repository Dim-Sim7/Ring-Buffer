#pragma once
#include <cstddef>
#include <vector>

/**
    Naive ring buffer implementation
    Using vector instead of array
    Not thread safe
    False sharing
    one unused data slot
    slow modulus wrap around
    No caching reads and writes idx

*/
template<typename T, size_t Capacity>
class RingBuffer {
public:
    RingBuffer() : data_(Capacity, 0) {}
    
    void push(const T& item);
    void pop(T& item);
    void debug() const;
private:
    size_t readIdx_{0};
    size_t writeIdx_{0};
    std::vector<T> data_;
};

template<typename T, size_t Capacity>
void RingBuffer<T, Capacity>::push(const T& item) {
    size_t nextWrite = (writeIdx_ + 1) % Capacity;

    // buffer full and oldest data is going to be overwritten, advance readIdx_
    if (nextWrite == readIdx_) {
        readIdx_ = (readIdx_ + 1) % Capacity;
    }
    data_[writeIdx_] = item;
    writeIdx_ = nextWrite;
}

template<typename T, size_t Capacity>
void RingBuffer<T, Capacity>::pop(T& item) {
    // if empty, no read allowed
    if (readIdx_ == writeIdx_) {
        return;
    }

    item = data_[readIdx_];
    readIdx_ = (readIdx_ + 1) % Capacity;
}

void RingBuffer::debug() const {
        std::cout << "Buffer: ";

        for (const auto& item : data_) {
            std::cout << item << ' ';
        }

        std::cout << "\nreadIdx: " << readIdx_
                << " writeIdx: " << writeIdx_
                << "\n\n";
    }