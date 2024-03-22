#ifndef __DYNBAR_FLATDYNAMICBARRIER_HPP__
#define __DYNBAR_FLATDYNAMICBARRIER_HPP__

#include <cstdint>
#include <atomic>
#include <concepts>

namespace DYNBAR
{
    template <std::unsigned_integral T>
    class FlatDynamicBarrier
    {
        private:
            struct alignas(2 * sizeof(T)) Payload
            {
                T target;
                T count;

                Payload() : target(0), count(0)
                {
                }

                Payload(T count, T target) : target(target), count(count)
                {
                }
            };

            std::atomic<Payload> payload;

        public:
            FlatDynamicBarrier() : payload(Payload(0, 0))
            {
            }

            void IncrementTarget()
            {
                // Can only increment the target if the barrier is NOT in use (i.e., count == 0).
                Payload old_payload = this->payload.load();
                old_payload.count = 0;
                Payload new_payload = old_payload;
                new_payload.target++;
                while (!this->payload.compare_exchange_weak(old_payload, new_payload))
                {
                    old_payload.count = 0;
                    new_payload = old_payload;
                    new_payload.target++;
                }
            }

            void DecrementTarget()
            {
                // To avoid deadlocks, decrementing a target can happen at any time, as long as count is less than target.
                // To elaborate on deadlocks, imaging the following scenario:
                // 1. Thread 1 enters.
                // 2. Thread 2 tries to decrement, has to wait for all to exit barrier.
                // 3. Thread 1 will never exit barrier because it is waiting for thread 2 to enter.
                // 4. Deadlock.
                Payload old_payload = this->payload.load();
                while (old_payload.count == old_payload.target)
                {
                    old_payload = this->payload.load();
                }
                Payload new_payload = old_payload;
                new_payload.target--;
                while (!this->payload.compare_exchange_weak(old_payload, new_payload))
                {
                    while (old_payload.count == old_payload.target)
                    {
                        old_payload = this->payload.load();
                    }
                    new_payload = old_payload;
                    new_payload.target--;
                }
            }

            T Arrive()
            {
                // Enter the barrier
                Payload old_payload = this->payload.load();
                Payload new_payload = old_payload;
                new_payload.count++;
                while (!this->payload.compare_exchange_weak(old_payload, new_payload))
                {
                    new_payload = old_payload;
                    new_payload.count++;
                }
                if (old_payload.count == 0)
                {
                    // First one to enter, wait for target to be reached then reset count.
                    old_payload.count = old_payload.target;
                    new_payload.count = 0;
                    while (!this->payload.compare_exchange_weak(old_payload, new_payload))
                    {
                        old_payload.count = old_payload.target;
                        new_payload.target = old_payload.target;
                        new_payload.count = 0;
                    }
                    return 0;
                }
                else
                {
                    // Not the first one to enter, wait for count to be reset.
                    while (this->payload.load().count != 0);
                    // Return order of entry.
                    return old_payload.count;
                }
            }
    };
}

#endif //__DYNBAR_FLATDYNAMICBARRIER_HPP__
