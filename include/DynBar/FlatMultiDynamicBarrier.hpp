#ifndef __DYNBAR_FLATMULTIDYNAMICBARRIER_HPP__
#define __DYNBAR_FLATMULTIDYNAMICBARRIER_HPP__

#include <cstdint>
#include <atomic>
#include <concepts>

namespace DYNBAR
{
    template <std::unsigned_integral T>
    class FlatMultiDynamicBarrier
    {
        private:
            enum class State : uint8_t
            {
                ENTERING = 0,
                EXITING = 1,
            };
            struct alignas(2 * sizeof(T)) Payload
            {
                State state : 1;
                uint8_t index : 7;
                T threads : sizeof(T) * 8 - 4;
                T waiting : sizeof(T) * 8 - 4;

                Payload() : state(State::ENTERING), index(0), threads(0), waiting(0)
                {
                }

                Payload(T waiting, uint8_t index, T threads) : state(State::ENTERING), index(index), threads(threads), waiting(waiting)
                {
                }
            };

            const T max_threads;
            const uint8_t max_barriers;
            std::atomic<Payload> payload;

        public:
            explicit FlatMultiDynamicBarrier(uint8_t max_barriers, T max_threads) : max_threads(max_threads),
                               max_barriers(max_barriers), payload()
            {
            }

            FlatMultiDynamicBarrier(uint8_t max_barriers, T max_threads, T opted_in_threads) : max_threads(max_threads),
                               max_barriers(max_barriers), payload(Payload(0, 0, opted_in_threads))
            {
            }

            void OptIn()
            {
                // Can only increment the threads if the barrier is NOT in use (i.e., waiting == 0, index = 0,
                // and state is ENTERING).
                Payload old_payload = this->payload.load();
                old_payload.waiting = 0;
                old_payload.index = 0;
                old_payload.state = State::ENTERING;
                Payload new_payload = old_payload;
                new_payload.threads++;
                while (!this->payload.compare_exchange_weak(old_payload, new_payload))
                {
                    old_payload.waiting = 0;
                    old_payload.index = 0;
                    old_payload.state = State::ENTERING;
                    new_payload = old_payload;
                    new_payload.threads++;
                }
            }

            void OptOut()
            {
                // To avoid deadlocks, decrementing threads can happen at any time the state is ENTERING, as long as
                // waiting is less than threads and index is 0.
                // To elaborate on deadlocks, imaging the following scenario:
                // 1. Thread 1 enters.
                // 2. Thread 2 tries to decrement, has to wait for all to exit barrier.
                // 3. Thread 1 will never exit barrier because it is waiting for thread 2 to enter.
                // 4. Deadlock.
                Payload old_payload = this->payload.load();
                while (old_payload.waiting == old_payload.threads || old_payload.state == State::EXITING ||
                       old_payload.index != 0)
                {
                    old_payload = this->payload.load();
                }
                Payload new_payload = old_payload;
                new_payload.threads--;
                // If after decrementing, waiting is equal to threads, set state to EXITING.
                if (new_payload.waiting == new_payload.threads && new_payload.threads != 0)
                {
                    new_payload.state = State::EXITING;
                }
                while (!this->payload.compare_exchange_weak(old_payload, new_payload))
                {
                    while (old_payload.waiting == old_payload.threads || old_payload.state == State::EXITING ||
                           old_payload.index != 0)
                    {
                        old_payload = this->payload.load();
                    }
                    new_payload = old_payload;
                    new_payload.threads--;
                    if (new_payload.waiting == new_payload.threads && new_payload.threads != 0)
                    {
                        new_payload.state = State::EXITING;
                    }
                }
            }

            void Arrive(uint8_t index)
            {
                // Enter the barrier, barrier must be in ENTERING state and index must match.
                Payload old_payload = this->payload.load();
                old_payload.state = State::ENTERING;
                old_payload.index = index;
                Payload new_payload = old_payload;
                new_payload.waiting++;
                // If we are last to enter, set state to EXITING.
                if (new_payload.waiting == new_payload.threads)
                {
                    new_payload.state = State::EXITING;
                }
                while (!this->payload.compare_exchange_weak(old_payload, new_payload))
                {
                    old_payload.state = State::ENTERING;
                    old_payload.index = index;
                    new_payload = old_payload;
                    new_payload.waiting++;
                    if (new_payload.waiting == new_payload.threads)
                    {
                        new_payload.state = State::EXITING;
                    }
                }
                // Wait for all threads to enter (state becomes EXITING).
                while (this->payload.load().state == State::ENTERING);
                // Then decrement the waiting.
                old_payload = this->payload.load();
                new_payload = old_payload;
                new_payload.waiting--;
                // If we are last to exit, set state to ENTERING and increment the index.
                if (new_payload.waiting == 0)
                {
                    new_payload.state = State::ENTERING;
                    new_payload.index++;
                    if (new_payload.index == this->max_barriers)
                    {
                        new_payload.index = 0;
                    }
                }
                while (!this->payload.compare_exchange_weak(old_payload, new_payload))
                {
                    new_payload = old_payload;
                    new_payload.waiting--;
                    if (new_payload.waiting == 0)
                    {
                        new_payload.state = State::ENTERING;
                        new_payload.index++;
                        if (new_payload.index == this->max_barriers)
                        {
                            new_payload.index = 0;
                        }
                    }
                }
            }

            T GetMaxThreads() const
            {
                return this->max_threads;
            }

            T GetOptedInThreads() const
            {
                return this->payload.load().threads;
            }

            T GetWaitingThreads() const
            {
                return this->payload.load().waiting;
            }

            uint8_t GetMaxBarriers() const
            {
                return this->max_barriers;
            }
    };
}

#endif //__DYNBAR_FLATMULTIDYNAMICBARRIER_HPP__
