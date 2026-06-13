#include <iostream>
#include "ringbuffernaive.h"

int main() {
    RingBuffer<int, 4> rb;
    rb.debug();
    std::cout << "Pushing values...\n";

    if (rb.push(1)) std::cout <<  "Pushed 1\n";
    rb.debug();
    if (rb.push(2)) std::cout <<  "Pushed 2\n";
    rb.debug();
    if (rb.push(3)) std::cout <<  "Pushed 3\n";
    rb.debug();
    if (rb.push(4)) std::cout <<  "Pushed 4\n";
    rb.debug();
    // destructive push overwrites oldest
    rb.push(5);
    rb.debug();

    int value;

    std::cout << "\nPopping values...\n";

    while (rb.pop(value)) {
        std::cout << value << '\n';
        rb.debug();
    }
    rb.debug();
    return 0;
}

