#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <type_traits>
#include <memory>
#include <chrono>

class ThreadPool {
public:
    
    
    explicit ThreadPool(size_t numThreads) : m_stop(false) {
        for (size_t i = 0; i < numThreads; ++i) {
            m_workers.emplace_back([this] { workerLoop(); });
        }
    }

    ~ThreadPool() {
        shutdown();
    }

    // Disable copying to avoid resource management issues
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // ====================================================================
    // Task Submission
    // ====================================================================

    /**
     * Submits a generic task to the pool.
     * 
     * @tparam F    The function type
     * @tparam Args The argument types
     * @return std::future<ReturnType> allowing the caller to wait for the result
     */
    template<class F, class... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using ReturnType = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<ReturnType> result = task->get_future();

        {
            std::scoped_lock lock(m_mutex);
            
            // Don't allow submission if stopping
            if (m_stop) {
                throw std::runtime_error("ThreadPool is stopped. Cannot submit new tasks.");
            }

            // Wrap the packaged_task in a void lambda since our queue is purely generic
            m_tasks.emplace([task]() {
                (*task)();
            });
            
            m_stats.totalSubmitted++;
        }

        m_cv.notify_one();
        return result;
    }



    void printStats() const {
        // Queue size requires locking for accuracy
        size_t queueSize = 0;
        
        std::cout << "--- ThreadPool Stats ---\n";
        std::cout << "Tasks Submitted: " << m_stats.totalSubmitted.load() << "\n";
        std::cout << "Tasks Completed: " << m_stats.totalCompleted.load() << "\n";
        // We do not lock here for performance; queue size is an approximation during runtime
        std::cout << "Approx Queue Size: " << (m_stats.totalSubmitted.load() - m_stats.totalCompleted.load()) << " (Derived)\n";
        std::cout << "------------------------\n";
    }

private:
  

    // Task Queue: Stores generic void functions (type-erased wrappers)
    std::queue<std::function<void()>> m_tasks;
    
    // Workers
    std::vector<std::thread> m_workers;

    // Synchronization
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop;

    // Statistics
    struct Stats {
        std::atomic<size_t> totalSubmitted{0};
        std::atomic<size_t> totalCompleted{0};
    } m_stats;


    // Helper Methods
    

    void workerLoop() {
        while (true) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(m_mutex);
                
                // Wait until there is work OR we are stopping
                m_cv.wait(lock, [this] { 
                    return m_stop || !m_tasks.empty(); 
                });

           
                if (m_stop && m_tasks.empty()) {
                    return;
                }

                task = std::move(m_tasks.front());
                m_tasks.pop();
            }

            // Execute task outside lock
            task();
            
            m_stats.totalCompleted++;
        }
    }

    void shutdown() {
        {
            std::scoped_lock lock(m_mutex);
            if (m_stop) return; // Already stopped
            m_stop = true;
        }

        m_cv.notify_all();

        for (auto& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
};



int main() {
    std::cout << "Initializing ThreadPool with 4 threads...\n";
    ThreadPool pool(4);

    // 1. Submit a task that returns a value (int)
    std::cout << "Submitting calculation tasks...\n";
    std::vector<std::future<int>> results;
    
    for (int i = 0; i < 8; ++i) {
        results.emplace_back(
            pool.submit([i] {
                // Simulate CPU work
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                return i * i;
            })
        );
    }

    // 2. Submit a void task
    pool.submit([] {
        std::cout << "  [Task] Logging from a worker thread.\n";
    });

    // 3. Process results
    int sum = 0;
    for (auto& f : results) {
        sum += f.get(); // waits for the result
    }
    std::cout << "Sum of squares (0..7): " << sum << "\n";

    // 4. Print Statistics
    // Wait briefly to ensure the logging task finishes (just for stats accuracy in output)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pool.printStats();

    std::cout << "Main exiting. Destructor will handle graceful shutdown...\n";
    return 0;
}
