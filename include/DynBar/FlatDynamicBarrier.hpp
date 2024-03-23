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
            enum class State : uint8_t
            {
                ENTERING = 0,
                EXITING = 1,
            };
            struct alignas(2 * sizeof(T)) Payload
            {
                State state : 1;
                T target : sizeof(T) * 8 - 1;
                T count : sizeof(T) * 8;

                Payload() : state(State::ENTERING), target(0), count(0)
                {
                }

                Payload(T count, T target) : state(State::ENTERING), target(target), count(count)
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
                // Can only increment the target if the barrier is NOT in use (i.e., count == 0 and state is ENTERING).
                Payload old_payload = this->payload.load();
                old_payload.count = 0;
                old_payload.state = State::ENTERING;
                Payload new_payload = old_payload;
                new_payload.target++;
                while (!this->payload.compare_exchange_weak(old_payload, new_payload))
                {
                    old_payload.count = 0;
                    old_payload.state = State::ENTERING;
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
                // Enter the barrier, barrier must be in ENTERING state.
                Payload old_payload = this->payload.load();
                old_payload.state = State::ENTERING;
                Payload new_payload = old_payload;
                new_payload.count++;
                // If we are last to enter, set state to EXITING.
                if (new_payload.count == new_payload.target)
                {
                    new_payload.state = State::EXITING;
                }
                while (!this->payload.compare_exchange_weak(old_payload, new_payload))
                {
                    old_payload.state = State::ENTERING;
                    new_payload = old_payload;
                    new_payload.count++;
                    if (new_payload.count == new_payload.target)
                    {
                        new_payload.state = State::EXITING;
                    }
                }
                T order = old_payload.count;
                // Wait for all threads to enter (state becomes EXITING).
                while (this->payload.load().state == State::ENTERING);
                // Then decrement the count.
                old_payload = this->payload.load();
                new_payload = old_payload;
                new_payload.count--;
                // If we are last to exit, set state to ENTERING.
                if (new_payload.count == 0)
                {
                    new_payload.state = State::ENTERING;
                }
                while (!this->payload.compare_exchange_weak(old_payload, new_payload))
                {
                    new_payload = old_payload;
                    new_payload.count--;
                    if (new_payload.count == 0)
                    {
                        new_payload.state = State::ENTERING;
                    }
                }
                return order;
            }
    };
}

#endif //__DYNBAR_FLATDYNAMICBARRIER_HPP__
