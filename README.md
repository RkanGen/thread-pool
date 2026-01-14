# thread-pool
A lightweight, header-only, standard C++ implementation of a fixed-size thread pool. It supports asynchronous task submission with return values, thread-safe queuing, and graceful shutdown.


##  Features

- **Modern C++17**
  - Uses `std::invoke_result_t`, `std::scoped_lock`, and generic lambdas
- **Zero Dependencies**
  - Built entirely using the Standard Template Library (STL)
- **Asynchronous Results**
  - Every submitted task returns a `std::future<>`
- **Graceful Shutdown (RAII)**
  - Ensures all queued tasks complete before program exit
- **Performance Metrics**
  - Tracks submitted and completed tasks using lock-free atomics
- **CPU Efficient**
  - Uses `std::condition_variable` to put worker threads to sleep when idle (no busy-waiting)

---



### 1️⃣ The Worker Team

When the `ThreadPool` is constructed, it launches a fixed number of `std::thread` workers.

Each worker enters a continuous loop:
- Sleeps while no tasks are available
- Wakes up when notified
- Executes tasks from the shared queue

---

### 2️⃣ The Task Wrapper (Type Erasure)

Since C++ is strongly typed but the pool must store different callable types in one queue, **type erasure** is used.

**Submission Flow:**
1. The submitted function is wrapped in a `std::packaged_task`
2. A `std::future` is extracted and returned immediately to the caller
3. The task is wrapped again inside a `void()` lambda
4. The lambda is pushed into a `std::queue<std::function<void()>>`

This allows the pool to execute any callable while preserving return values.

---

### 3️⃣ Synchronization

- **Mutual Exclusion**
  - A `std::mutex` protects the task queue from race conditions
- **Efficient Signaling**
  - A `std::condition_variable` is used to wake sleeping workers
  - `notify_one()` wakes exactly one worker per submitted task

---

### 4️⃣ Shutdown Lifecycle (RAII)

Cleanup is handled automatically in the destructor:

1. A `stop` flag is set to prevent new submissions
2. All worker threads are notified
3. Workers:
   - Wake up
   - Continue processing remaining tasks
   - Exit once the queue is empty
4. The main thread joins all workers safely

This guarantees **no lost tasks and no dangling threads**.

---

##  Quick Start

### 1️⃣ Compile

Make sure to use C++17 and link against `pthread`.

```bash
g++ -std=c++17 -pthread main.cpp -o threadpool
./threadpool
````

---

### 2️⃣ Usage Example

```cpp
#include "ThreadPool.cpp" // Assuming implementation is in this file

int main() {
    // 1. Create a pool with 4 worker threads
    ThreadPool pool(4);

    // 2. Submit a task that returns a value
    auto futureResult = pool.submit([] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 42;
    });

    // 3. Submit a void task (fire-and-forget)
    pool.submit([] {
        std::cout << "Job done!\n";
    });

    // 4. Retrieve the result
    std::cout << "Result: " << futureResult.get() << "\n";

    // 5. Print thread pool statistics
    pool.printStats();

    return 0;
} 
// Destructor triggers automatically here, finishing remaining tasks
```

---

##  Performance Metrics

The thread pool maintains lightweight internal statistics using `std::atomic` to minimize locking overhead:

* **Total Submitted Tasks**

  * Incremented on each `submit()` call
* **Total Completed Tasks**

  * Incremented after task execution
* **Queue Size**

  * Calculated dynamically as:

    ```
    queue_size = submitted - completed
    ```


Just say which one ⚡
```
