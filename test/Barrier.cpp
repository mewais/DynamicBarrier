#include <iostream>
#include <thread>
#include <barrier>
#include <string>

#include "DynBar/FlatDynamicBarrier.hpp"

#define THREAD_COUNT 4
#define ITERATIONS 10000

DYNBAR::FlatDynamicBarrier<uint8_t> barrier;
std::barrier std_barrier(THREAD_COUNT);

void thread(uint32_t tid)
{
    // The goal here is to test the basic barrier functionality, not the increment/decrement functionality.
    // So increment, then use std barrier to make sure everyone incremented before starting the test.
    barrier.IncrementTarget();
    std::string str;
    std_barrier.arrive_and_wait();
    for (int i = 0; i < ITERATIONS; i++)
    {
        uint8_t arrival = barrier.Arrive();
        str = "Thread " + std::to_string(tid) + " iteration " + std::to_string(i) + " arrived " + std::to_string(arrival) + "\n";
        std::cout << str;
    }
    barrier.DecrementTarget();
}

int main()
{
    std::thread threads[THREAD_COUNT];
    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        threads[i] = std::thread(thread, i);
    }
    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        threads[i].join();
    }
    return 0;
}