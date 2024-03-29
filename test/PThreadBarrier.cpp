#include <thread>
#include <string>
#include <vector>
#ifndef NDEBUG
#include <iostream>
#endif // NDEBUG

#include <pthread.h>

uint32_t thread_count;
uint32_t iterations;

pthread_barrier_t barrier;

void thread(uint32_t tid)
{
    // The goal here is to test the basic barrier functionality, not the increment/decrement functionality.
    // So increment, then use std barrier to make sure everyone incremented before starting the test.
#ifndef NDEBUG
    std::string str;
#endif // NDEBUG
    for (uint32_t i = 0; i < iterations; i++)
    {
        pthread_barrier_wait(&barrier);
#ifndef NDEBUG
        str = "Thread " + std::to_string(tid) + " iteration " + std::to_string(i) + "\n";
        std::cout << str;
#endif // NDEBUG
    }
}

int main(int argc, char** argv)
{
    thread_count = std::stoi(argv[1]);
    iterations = std::stoi(argv[2]);

    pthread_barrier_init(&barrier, NULL, thread_count);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < thread_count; i++)
    {
        threads.emplace_back(std::thread(thread, i));
    }
    for (uint32_t i = 0; i < thread_count; i++)
    {
        threads[i].join();
    }
    pthread_barrier_destroy(&barrier);
    return 0;
}
