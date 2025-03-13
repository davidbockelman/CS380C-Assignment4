#include <iostream>
#include <atomic>
#include <thread>

std::atomic<int> counter(0); // Atomic counter

void incrementCounter() {
    for (int i = 0; i < 1000; ++i) {
        counter.fetch_add(1, std::memory_order_relaxed); // Atomic increment
    }
}

void decrementCounter() {
    for (int i = 0; i < 1000; ++i) {
        counter.fetch_sub(1, std::memory_order_relaxed); // Atomic decrement
    }
}

int main() {
    std::thread t1(incrementCounter); // Increment thread
    std::thread t2(decrementCounter); // Decrement thread

    t1.join(); // Wait for increment thread to finish
    t2.join(); // Wait for decrement thread to finish

    std::cout << "Final counter value: " << counter.load(std::memory_order_relaxed) << std::endl;
    return 0;
}

