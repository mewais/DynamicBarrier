#include <iostream>
#include <thread>
#include <barrier>
#include <string>

#include "DynamicBarrier.hpp"

#define THREAD_COUNT 4
#define ITERATIONS 10000

DYNBAR::DynamicBarrier barrier;
std::barrier std_barrier(THREAD_COUNT);

void thread(uint32_t tid)
{
    // The goal here is to test the basic barrier functionality, not the increment/decrement functionality.
    // So increment, then use std barrier to make sure everyone incremented before starting the test.
    barrier.IncrementTarget();
    std::string str = "Thread " + std::to_string(tid) + " iteration ";
    std::string str2;
    std_barrier.arrive_and_wait();
    for (int i = 0; i < ITERATIONS; i++)
    {
        str2 = str + std::to_string(i) + "\n";
        std::cout << str2;
        barrier.Arrive();
    }
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