# Dynamic Barrier
This is no regular barrier. This is a dynamic barrier that allows threads to opt in/out of the barrier at runtime. The barrier is implemented purely using atomics.

## Types
- `FlatDynamicBarrier`: This is your typical barrier where all threads must reach the barrier before any thread can proceed.
- `TreeDynamicBarrier`: I noticed that with a large number of threads, the `FlatDynamicBarrier` was not as efficient as I would have liked. Especially since I use these mostly in loops. So I implemented a tree barrier where every group of threads meet at a leaf barrier, and only one of them proceeds to the next level. This is repeated until all threads have reached the top level. This is supposed to decrease ping-ponging of the atomic variable and decrease contention as a whole.

## Size
The barrier is templated to allow you to use any number of threads. The following values are allowed:
- `uint8_t`: 0-128 threads
- `uint16_t`: 0-32768 threads
- `uint32_t`: 0-2147483648 threads

## Usage
```cpp
#include "DynBar/FlatDynamicBarrier.hpp"
#include "DynBar/TreeDynamicBarrier.hpp"

FlatDynamicBarrier<uint8_t> barrier(4); // 4 threads
barrrier.IncrementTarget(); // Increment the target by 1
barrier.DecrementTarget(); // Decrement the target by 1
barrier.Arrive(); // Wait for all threads to reach the barrier

TreeDynamicBarrier<uint8_t> barrier(4); // 4 threads
barrrier.IncrementTarget(); // Increment the target by 1
barrier.DecrementTarget(); // Decrement the target by 1
barrier.Arrive(); // Wait for all threads to reach the barrier
```

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
