#include <thread>
#include <string>
#include <vector>
#ifndef NDEBUG
#include <iostream>
#endif // NDEBUG

#include "DynBar/TreeMultiDynamicBarrier.hpp"

uint32_t thread_count;
uint32_t iterations;

DYNBAR::TreeMultiDynamicBarrier* barrier;

void thread(uint32_t tid)
{
    // The goal here is to test the basic barrier functionality, not the increment/decrement functionality.
    // So increment, then use std barrier to make sure everyone incremented before starting the test.
#ifndef NDEBUG
    std::string str;
#endif // NDEBUG
    for (uint32_t i = 0; i < iterations; i++)
    {
        barrier->Arrive(tid, 0);
#ifndef NDEBUG
        str = "Thread " + std::to_string(tid) + " iteration " + std::to_string(i) + " barrier 1\n";
        std::cout << str;
#endif // NDEBUG
        barrier->Arrive(tid, 1);
#ifndef NDEBUG
        str = "Thread " + std::to_string(tid) + " iteration " + std::to_string(i) + " barrier 2\n";
        std::cout << str;
#endif // NDEBUG
    }
}

int main(int argc, char** argv)
{
    thread_count = std::stoi(argv[1]);
    iterations = std::stoi(argv[2]);

    barrier = new DYNBAR::TreeMultiDynamicBarrier(2, 2, thread_count, thread_count);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < thread_count; i++)
    {
        threads.emplace_back(std::thread(thread, i));
    }
    for (uint32_t i = 0; i < thread_count; i++)
    {
        threads[i].join();
    }
    delete barrier;
    return 0;
}