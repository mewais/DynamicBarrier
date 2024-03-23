#include <iostream>
#include <thread>
#include <string>
#include <cstdlib>
#include <ctime>

#include "DynBar/FlatDynamicBarrier.hpp"

#define THREAD_COUNT 4
#define ITERATIONS 10000
#define FREQUENCY 100           // How often should we decrement from the barrier
#define LENGTH 5                // How long should a thread spen unbarriered


DYNBAR::FlatDynamicBarrier<uint8_t> barrier(THREAD_COUNT);

void thread(uint32_t tid)
{
    srand(time(nullptr));
    bool use_barrier = true;
    uint32_t length = 0;
    std::string str;
    barrier.OptIn();
    for (int i = 0; i < ITERATIONS; i++)
    {
        if (use_barrier)
        {
            if ((rand() % FREQUENCY) == 0)
            {
                barrier.OptOut();
                use_barrier = false;
                length = LENGTH;
                str = "Thread " + std::to_string(tid) + " iteration " + std::to_string(i) + " did not use barrier\n";
            }
            else
            {
                uint8_t arrival = barrier.Arrive();
                str = "Thread " + std::to_string(tid) + " iteration " + std::to_string(i) + " arrived " + std::to_string(arrival) + "\n";
            }
        }
        else
        {
            length--;
            if (length == 0)
            {
                barrier.OptIn();
                use_barrier = true;
            }
            str = "Thread " + std::to_string(tid) + " iteration " + std::to_string(i) + " did not use barrier\n";
        }
        std::cout << str;
    }
    if (use_barrier)
    {
        barrier.OptOut();
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