#ifndef __DYNBAR_TREEDYNAMICBARRIER_HPP__
#define __DYNBAR_TREEDYNAMICBARRIER_HPP__

#include <cstdint>
#include <atomic>
#include <cmath>
#include <functional>
#include <mutex>

namespace DYNBAR
{
    class TreeDynamicBarrier
    {
        private:
            enum class State : uint8_t
            {
                ENTERING = 0,
                EXITING = 1,
            };
            struct Payload
            {
                State state : 1;
                uint8_t threads : 3;
                uint8_t waiting : 4;

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
            uint32_t leaf_nodes;

            std::mutex opt_in_mutex;

            // Atomics are not copyable, so we need to use a pointer to an atomic
            std::atomic<Payload>** payload_tree;

        public:
            TreeDynamicBarrier(uint32_t max_threads, uint32_t node_size) : max_threads(max_threads),
                               node_size(node_size), tree_depth(std::log(max_threads + 1) / std::log(node_size)),
                               shift_amount(std::log2(node_size))
            {
                // Node size must be a power of 2
                if ((node_size & (node_size - 1)) != 0)
                {
                    throw std::invalid_argument("Node size must be a power of 2");
                }
                // Until we template the payload, we need to make sure the node size is not too big
                if (node_size > 8)
                {
                    throw std::invalid_argument("Node size must be less than or equal to 8");
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
                    this->leaf_nodes = nodes;           // Will keep getting updated until the last level
                    this->payload_tree[i] = new std::atomic<Payload>[nodes];
                    // Initialize every node in the level
                    for (uint32_t j = 0; j < nodes; j++)
                    {
                        this->payload_tree[i][j].store(Payload(0, 0));
                    }
                }
            }

            TreeDynamicBarrier(uint32_t max_threads, uint32_t opted_in_threads, uint32_t node_size) :
                               max_threads(max_threads), node_size(node_size),
                               tree_depth(std::log(max_threads + 1) / std::log(node_size)),
                               shift_amount(std::log2(node_size))
            {
                // Node size must be a power of 2
                if ((node_size & (node_size - 1)) != 0)
                {
                    throw std::invalid_argument("Node size must be a power of 2");
                }
                // Until we template the payload, we need to make sure the node size is not too big
                if (node_size > 8)
                {
                    throw std::invalid_argument("Node size must be less than or equal to 8");
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
                        this->payload_tree[i][j].store(Payload(0, 0));
                    }
                }
                // Opt in the specified number of threads
                for (uint32_t i = 0; i < opted_in_threads; i++)
                {
                    this->OptIn(i);
                }
            }

            ~TreeDynamicBarrier()
            {
                for (int32_t i = this->tree_depth - 1; i >= 0; i--)
                {
                    delete[] this->payload_tree[i];
                }
                delete[] this->payload_tree;
            }

            void OptIn(uint32_t tid)
            {
                // Can only increment if the barrier is NOT in use (i.e., waiting == 0 and state is ENTERING).
                // The problem is, the barrier is no longer one unit, and while we can be sure that a leaf node is not
                // in use, we cannot be sure that the parent node is not in use. Doing it step by step exposes us to races
                // if/when multiple threads try to opt in in the same node.
                // To do it all at once, we will use a mutex (opting in is much less frequent than arriving, so I will
                // bite the bullet). We will lock the whole thing, preventing other threads from opting in, then do the
                // opt in step by step
                this->opt_in_mutex.lock();
                uint32_t node = tid >> this->shift_amount;
                int32_t level = this->tree_depth - 1;
                while (level >= 0)
                {
                    std::atomic<Payload>& node_payload = this->payload_tree[level][node];
                    Payload old_payload = node_payload.load();
                    old_payload.waiting = 0;
                    old_payload.state = State::ENTERING;
                    Payload new_payload = old_payload;
                    new_payload.threads++;
                    while (!node_payload.compare_exchange_weak(old_payload, new_payload))
                    {
                        old_payload.waiting = 0;
                        old_payload.state = State::ENTERING;
                        new_payload = old_payload;
                        new_payload.threads++;
                    }
                    if (old_payload.threads != 0)
                    {
                        break;
                    }
                    // We were at 0, must increment parent
                    level--;
                    node >>= this->shift_amount;
                }
                this->opt_in_mutex.unlock();
            }

            void OptOut(uint32_t tid)
            {
                // To avoid deadlocks, decrementing threads can happen at any time the state is ENTERING, as long as
                // waiting is less than threads.
                // To elaborate on deadlocks, imaging the following scenario:
                // 1. Thread 1 enters.
                // 2. Thread 2 tries to decrement, has to wait for all to exit barrier.
                // 3. Thread 1 will never exit barrier because it is waiting for thread 2 to enter.
                // 4. Deadlock.

                // We do the following:
                // 1. Find the leaf node we are at.
                // 2. If the state is EXITING, wait for it to become ENTERING.
                // 3. If the state is ENTERING, decrement the threads.
                // 4. If after decrementing, waiting is equal to threads, set state to EXITING.
                // 5. If after decrementing, number of threads is 0, also decrement the parent node. Repeat if needed
                uint32_t node = tid >> this->shift_amount;
                int32_t level = this->tree_depth - 1;

                while (level >= 0)
                {
                    std::atomic<Payload>& node_payload = this->payload_tree[level][node];
                    Payload old_payload = node_payload.load();
                    while (old_payload.waiting == old_payload.threads || old_payload.state == State::EXITING)
                    {
                        old_payload = node_payload.load();
                    }
                    Payload new_payload = old_payload;
                    new_payload.threads--;
                    // If after decrementing, waiting is equal to threads, set state to EXITING.
                    if (new_payload.waiting == new_payload.threads && new_payload.threads != 0)
                    {
                        new_payload.state = State::EXITING;
                    }
                    while (!node_payload.compare_exchange_weak(old_payload, new_payload))
                    {
                        while (old_payload.waiting == old_payload.threads || old_payload.state == State::EXITING)
                        {
                            old_payload = node_payload.load();
                        }
                        new_payload = old_payload;
                        new_payload.threads--;
                        if (new_payload.waiting == new_payload.threads && new_payload.threads != 0)
                        {
                            new_payload.state = State::EXITING;
                        }
                    }
                    // Decrement parent if needed
                    if (new_payload.threads == 0 && level > 0)
                    {
                        level--;
                        node >>= this->shift_amount;
                    }
                    else
                    {
                        break;
                    }
                }
            }

            void Arrive(uint32_t tid)
            {
                // We know the thread id, so we directly know the leaf node we should barrier at
                uint32_t node = tid >> this->shift_amount;
                int32_t level = this->tree_depth - 1;
                // From here, we can loop going up doing the following at every level:
                // 1. Enter the barrier, barrier must be in ENTERING state.
                // 2. If we are NOT the last to enter, wait for state to become EXITING.
                // 3. If we are the last to enter, traverse up the tree and repeat.
                // 4. If we are at the root level, and this is NOT the last thread to enter, wait for state to change to EXITING.
                // 5. If we are at the root level, and this is the last thread to enter, set state to EXITING.
                // 6. Traverse down the tree, setting state to EXITING at every level.

                while (level >= 0)
                {
                    std::atomic<Payload>& node_payload = this->payload_tree[level][node];
                    // Step 1
                    Payload old_payload = node_payload.load();
                    old_payload.state = State::ENTERING;
                    Payload new_payload = old_payload;
                    new_payload.waiting++;
                    while (!node_payload.compare_exchange_weak(old_payload, new_payload))
                    {
                        old_payload.state = State::ENTERING;
                        new_payload = old_payload;
                        new_payload.waiting++;
                    }

                    if (new_payload.waiting != new_payload.threads)
                    {
                        // Step 2 and 4
                        while (node_payload.load().state != State::EXITING);
                        old_payload = node_payload.load();
                        new_payload = old_payload;
                        new_payload.waiting--;
                        // If we are last to exit, set state to ENTERING.
                        if (new_payload.waiting == 0)
                        {
                            new_payload.state = State::ENTERING;
                        }
                        while (!node_payload.compare_exchange_weak(old_payload, new_payload))
                        {
                            new_payload = old_payload;
                            new_payload.waiting--;
                            if (new_payload.waiting == 0)
                            {
                                new_payload.state = State::ENTERING;
                            }
                        }
                        break;
                    }
                    else
                    {
                        if (level == 0)
                        {
                            // Step 5
                            old_payload = node_payload.load();
                            new_payload = old_payload;
                            new_payload.state = State::EXITING;
                            new_payload.waiting--;
                            // If we are last to exit, set state to ENTERING. (Happens if we are the only thread)
                            if (new_payload.waiting == 0)
                            {
                                new_payload.state = State::ENTERING;
                            }
                            while (!node_payload.compare_exchange_weak(old_payload, new_payload))
                            {
                                new_payload = old_payload;
                                new_payload.state = State::EXITING;
                                new_payload.waiting--;
                                if (new_payload.waiting == 0)
                                {
                                    new_payload.state = State::ENTERING;
                                }
                            }
                            break;
                        }
                        else
                        {
                            // Step 3
                            level--;
                            node >>= this->shift_amount;
                        }
                    }
                }

                // Step 6
                while (level < (int32_t)this->tree_depth - 1)
                {
                    level++;
                    node = tid >> (this->shift_amount * (this->tree_depth - level));
                    std::atomic<Payload>& node_payload = this->payload_tree[level][node];
                    Payload old_payload = node_payload.load();
                    Payload new_payload = old_payload;
                    new_payload.state = State::EXITING;
                    new_payload.waiting--;
                    // If we are last to exit, set state to ENTERING.
                    if (new_payload.waiting == 0)
                    {
                        new_payload.state = State::ENTERING;
                    }
                    while (!node_payload.compare_exchange_weak(old_payload, new_payload))
                    {
                        new_payload = old_payload;
                        new_payload.state = State::EXITING;
                        new_payload.waiting--;
                        if (new_payload.waiting == 0)
                        {
                            new_payload.state = State::ENTERING;
                        }
                    }
                }
            }

            uint32_t GetMaxThreads() const
            {
                return this->max_threads;
            }

            uint32_t GetNodeSize() const
            {
                return this->node_size;
            }

            uint32_t GetOptedInThreads() const
            {
                // Total number of threads of every node in the leafs
                uint32_t total_threads = 0;
                for (uint32_t i = 0; i < this->leaf_nodes; i++)
                {
                    total_threads += this->payload_tree[this->tree_depth - 1][i].load().threads;
                }
                return total_threads;
            }

            uint32_t GetWaitingThreads() const
            {
                // Total number of waiting threads of every node in the leafs
                uint32_t total_threads = 0;
                for (uint32_t i = 0; i < this->leaf_nodes; i++)
                {
                    total_threads += this->payload_tree[this->tree_depth - 1][i].load().waiting;
                }
                return total_threads;
            }
    };
}

#endif //__DYNBAR_TREEDYNAMICBARRIER_HPP__
