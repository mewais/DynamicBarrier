#include <thread>
#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>
#ifndef NDEBUG
#include <iostream>
#endif // NDEBUG

#include "DynBar/TreeDynamicBarrier.hpp"

uint32_t thread_count;
uint32_t iterations;

#define FREQUENCY 100           // How often should we decrement from the barrier
#define LENGTH 5                // How long should a thread spen unbarriered


DYNBAR::TreeDynamicBarrier* barrier;

void thread(uint32_t tid)
{
    srand(time(nullptr));
    bool use_barrier = true;
    uint32_t length = 0;
#ifndef NDEBUG
    std::string str;
#endif // NDEBUG
    barrier->OptIn(tid);
    for (uint32_t i = 0; i < iterations; i++)
    {
        if (use_barrier)
        {
            if ((rand() % FREQUENCY) == 0)
            {
                barrier->OptOut(tid);
                use_barrier = false;
                length = LENGTH;
#ifndef NDEBUG
                str = "Thread " + std::to_string(tid) + " iteration " + std::to_string(i) + " did not use barrier\n";
#endif // NDEBUG
            }
            else
            {
                barrier->Arrive(tid);
#ifndef NDEBUG
                str = "Thread " + std::to_string(tid) + " iteration " + std::to_string(i) + "\n";
#endif // NDEBUG
            }
        }
        else
        {
            length--;
            if (length == 0)
            {
                barrier->OptIn(tid);
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
        barrier->OptOut(tid);
    }
}

int main(int argc, char** argv)
{
    thread_count = std::stoi(argv[1]);
    iterations = std::stoi(argv[2]);

    barrier = new DYNBAR::TreeDynamicBarrier(2, thread_count);
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
