#include <thread>
#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>
#ifndef NDEBUG
#include <iostream>
#endif // NDEBUG

#include "DynBar/FlatMultiDynamicBarrier.hpp"

uint32_t thread_count;
uint32_t iterations;

#define FREQUENCY 100           // How often should we decrement from the barrier
#define LENGTH 5                // How long should a thread spen unbarriered


DYNBAR::FlatMultiDynamicBarrier<uint16_t>* barrier;

void thread(uint32_t tid)
{
    srand(time(nullptr));
    bool use_barrier = true;
    uint32_t length = 0;
#ifndef NDEBUG
    std::string str;
#endif // NDEBUG
    barrier->OptIn();
    for (uint32_t i = 0; i < iterations; i++)
    {
        if (use_barrier)
        {
            if ((rand() % FREQUENCY) == 0)
            {
                barrier->OptOut();
                use_barrier = false;
                length = LENGTH;
#ifndef NDEBUG
                str = "Thread " + std::to_string(tid) + " iteration " + std::to_string(i) + " did not use barrier\n";
#endif // NDEBUG
            }
            else
            {
                barrier->Arrive(0);
#ifndef NDEBUG
                str = "Thread " + std::to_string(tid) + " iteration " + std::to_string(i) + " barrier 1\n";
#endif // NDEBUG
                barrier->Arrive(1);
#ifndef NDEBUG
                str = "Thread " + std::to_string(tid) + " iteration " + std::to_string(i) + " barrier 2\n";
#endif // NDEBUG
            }
        }
        else
        {
            length--;
            if (length == 0)
            {
                barrier->OptIn();
                use_barrier = true;
            }
#ifndef NDEBUG
            str = "Thread " + std::to_string(tid) + " iteration " + std::to_string(i) + " did not use barrier\n";
#endif // NDEBUG
        }
#ifndef NDEBUG
        std::cout << str;
#endif // NDEBUG
    }
    if (use_barrier)
    {
        barrier->OptOut();
    }
}

int main(int argc, char** argv)
{
    thread_count = std::stoi(argv[1]);
    iterations = std::stoi(argv[2]);

    barrier = new DYNBAR::FlatMultiDynamicBarrier<uint16_t>(2, thread_count);
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
