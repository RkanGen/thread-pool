#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <chrono>

class ThreadPool {
public:
    // Constructor: Launches a fixed number of worker threads
    explicit ThreadPool(size_t numThreads) {
        start(numThreads);
    }

    // Destructor: Handles clean shutdown
    ~ThreadPool() {
        stop();
    }

    // Prevents copying to avoid undefined behavior with threads/mutexes
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Enqueues a task for execution
    void submit(std::function<void()> task) {
        {
            // Lock queue scope
            std::scoped_lock lock(m_mutex);
            m_tasks.emplace(std::move(task));
        }
        // Wake up one thread to handle the task
        m_cv.notify_one();
    }

private:
    // Core components
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    
    // Synchronization
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop = false;

    void start(size_t numThreads) {
        for (size_t i = 0; i < numThreads; ++i) {
            m_workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;

                    {
                        // Acquire lock to check queue and wait
                        std::unique_lock<std::mutex> lock(m_mutex);

                        // Wait until we have a task or we are stopping
                        m_cv.wait(lock, [this] {
                            return m_stop || !m_tasks.empty();
                        });

                        
                        if (m_stop && m_tasks.empty()) {
                            return;
                        }

                        // Get the task
                        task = std::move(m_tasks.front());
                        m_tasks.pop();
                    } 
                    
                    task();
                }
            });
        }
    }

    void stop() {
        {
            std::scoped_lock lock(m_mutex);
            m_stop = true;
        }
        
        // Wake up all threads so they can check the stop flag and exit
        m_cv.notify_all();

        // Wait for all threads to finish
        for (auto& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
};



int main() {
    std::cout << "Creating ThreadPool with 4 workers...\n";
    ThreadPool pool(4);

    // Submit 10 tasks
    for (int i = 0; i < 10; ++i) {
        pool.submit([i] {
            // Simulate work (e.g., printing and sleeping)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Safe printing (using a temporary string to minimize cout tearing)
            std::string msg = "Task " + std::to_string(i) + 
                              " executed by thread " + 
                              std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + "\n";
            std::cout << msg;
        });
    }

    std::cout << "All tasks submitted. Waiting for completion (via destructor)...\n";

    return 0;
}
