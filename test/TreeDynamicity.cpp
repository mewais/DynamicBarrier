#include <iostream>
#include <thread>
#include <string>
#include <cstdlib>
#include <ctime>

#include "DynBar/TreeDynamicBarrier.hpp"

#define THREAD_COUNT 16
#define ITERATIONS 10000
#define FREQUENCY 100           // How often should we decrement from the barrier
#define LENGTH 5                // How long should a thread spen unbarriered


DYNBAR::TreeDynamicBarrier barrier(THREAD_COUNT, 2);

void thread(uint32_t tid)
{
    srand(time(nullptr));
    bool use_barrier = true;
    uint32_t length = 0;
    std::string str;
    barrier.OptIn(tid);
    for (int i = 0; i < ITERATIONS; i++)
    {
        if (use_barrier)
        {
            if ((rand() % FREQUENCY) == 0)
            {
                barrier.OptOut(tid);
                use_barrier = false;
                length = LENGTH;
                str = "Thread " + std::to_string(tid) + " iteration " + std::to_string(i) + " did not use barrier\n";
            }
            else
            {
                barrier.Arrive(tid);
                str = "Thread " + std::to_string(tid) + " iteration " + std::to_string(i) + "\n";
            }
        }
        else
        {
            length--;
            if (length == 0)
            {
                barrier.OptIn(tid);
                use_barrier = true;
            }
            str = "Thread " + std::to_string(tid) + " iteration " + std::to_string(i) + " did not use barrier\n";
        }
        std::cout << str;
    }
    if (use_barrier)
    {
        barrier.OptOut(tid);
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