#ifndef __DYNBAR_TREEDYNAMICBARRIER_HPP__
#define __DYNBAR_TREEDYNAMICBARRIER_HPP__

#include <cstdint>
#include <atomic>
#include <cmath>
#include <functional>

namespace DYNBAR
{
    class TreeDynamicBarrier
    {
        private:
            enum class State : uint8_t
            {
                ENTERING = 0,
                WAITING = 1,
                EXITING = 2,
            };
            struct Payload
            {
                State state : 2;
                uint8_t threads : 3;
                uint8_t waiting : 3;

                Payload() : state(State::ENTERING), threads(0), waiting(0)
                {
                }

                Payload(uint8_t threads, uint8_t waiting) : state(State::ENTERING), threads(threads), waiting(waiting)
                {
                }
            };

            const uint32_t max_threads;
            const uint32_t node_size;
            const uint32_t tree_depth;

            const uint32_t shift_amount;            // The shift amount for the node size
            // Atomics are not copyable, so we need to use a pointer to an atomic
            std::atomic<Payload>** payload_tree;

        public:
            TreeDynamicBarrier(uint32_t max_threads, uint32_t node_size = 2) : max_threads(max_threads),
                               node_size(node_size), tree_depth(std::log(max_threads + 1) / std::log(node_size)),
                               shift_amount(std::log2(node_size))
            {
                // Node size must be a power of 2
                if ((node_size & (node_size - 1)) != 0)
                {
                    throw std::invalid_argument("Node size must be a power of 2");
                }
                // Allocate the tree
                this->payload_tree = new std::atomic<Payload>*[this->tree_depth];
                // Find how many nodes are in every level of the tree and allocate them
                for (uint32_t i = 0; i < this->tree_depth; i++)
                {
                    uint32_t nodes = 1;
                    for (uint32_t j = 0; j < i; j++)
                    {
                        nodes *= node_size;
                    }
                    this->payload_tree[i] = new std::atomic<Payload>[nodes];
                    // Initialize every node in the level
                    for (uint32_t j = 0; j < nodes; j++)
                    {
                        this->payload_tree[i][j].store(Payload(node_size, 0));
                    }
                }
            }

            ~TreeDynamicBarrier()
            {
                for (uint32_t i = 0; i < this->max_threads; i++)
                {
                    delete[] this->payload_tree[i];
                }
                delete[] this->payload_tree;
            }

            uint32_t Arrive(uint32_t tid)
            {
                // We know the thread id, so we directly know the leaf node we should barrier at
                uint32_t node = tid >> this->shift_amount;
                uint32_t level = this->tree_depth - 1;
                // From here, we can loop going up doing the following at every level:
                // 1. Enter the barrier, barrier must be in ENTERING state.
                // 2. If we are NOT the last to enter, wait for all threads to enter (state becomes WAITING).
                // 3. If we are the last to enter, set state to WAITING.
                // 4. If we are NOT the last to enter, wait for state to become EXITING.
                // 5. If we are the last to enter, traverse up the tree and repeat.
                // 6. If we are at the root level, and this is NOT the last thread to enter, wait for state to change from WAITING to EXITING.
                // 7. If we are at the root level, and this is the last thread to enter, set state to EXITING.
                // 8. Traverse down the tree, setting state to EXITING at every level.

                // Create lambda function for every level
                std::function<void(uint32_t, uint32_t)> LevelFunc;
                LevelFunc = [this, &LevelFunc](uint32_t level, uint32_t node) -> void
                {
                    // Step 1
                    Payload old_payload = this->payload_tree[level][node].load();
                    old_payload.state = State::ENTERING;
                    Payload new_payload = old_payload;
                    new_payload.waiting++;
                    // Step 3
                    if (new_payload.waiting == new_payload.threads)
                    {
                        new_payload.state = State::WAITING;
                    }
                    while (!this->payload_tree[level][node].compare_exchange_weak(old_payload, new_payload))
                    {
                        old_payload.state = State::ENTERING;
                        new_payload = old_payload;
                        new_payload.waiting++;
                        if (new_payload.waiting == new_payload.threads)
                        {
                            new_payload.state = State::WAITING;
                        }
                    }
                    if (new_payload.waiting != new_payload.threads)
                    {
                        // Step 2 merged into 4 (also, step 6)
                        while (this->payload_tree[level][node].load().state != State::EXITING);
                        // Step 8
                        old_payload = this->payload_tree[level][node].load();
                        new_payload = old_payload;
                        new_payload.waiting--;
                        // If we are last to exit, set state to ENTERING.
                        if (new_payload.waiting == 0)
                        {
                            new_payload.state = State::ENTERING;
                        }
                        while (!this->payload_tree[level][node].compare_exchange_weak(old_payload, new_payload))
                        {
                            new_payload = old_payload;
                            new_payload.waiting--;
                            if (new_payload.waiting == 0)
                            {
                                new_payload.state = State::ENTERING;
                            }
                        }
                    }
                    else
                    {
                        if (level != 0)
                        {
                            // Step 5, recursively call this
                            LevelFunc(level - 1, node >> this->shift_amount);
                        }
                        // Step 8 and 7
                        old_payload = this->payload_tree[level][node].load();
                        new_payload = old_payload;
                        new_payload.state = State::EXITING;
                        new_payload.waiting--;
                        while (!this->payload_tree[level][node].compare_exchange_weak(old_payload, new_payload))
                        {
                            old_payload = this->payload_tree[level][node].load();
                            new_payload = old_payload;
                            new_payload.state = State::EXITING;
                            new_payload.waiting--;
                        }
                    }
                };

                // Use it
                LevelFunc(level, node);
            }
    };
}

#endif //__DYNBAR_TREEDYNAMICBARRIER_HPP__
