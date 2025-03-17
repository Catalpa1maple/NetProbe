#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "common.h"
#include "net_probe.h"
#include <memory>
#include <functional>
#include <thread>

class ThreadPool {
public:
    // Constructor with default thread count
    explicit ThreadPool(size_t initialThreads = 8);
    
    // Destructor ensures proper cleanup
    ~ThreadPool();
    
    // Non-copyable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    
    // Enqueue a task with a client info object
    bool enqueue(std::shared_ptr<ClientInfo> clientInfo, 
                 std::function<void(std::shared_ptr<ClientInfo>)> func);
    
    // Get current thread statistics
    size_t getActiveThreads() const;
    size_t getPoolSize() const;

private:
    // Worker thread function
    void workerThread();
    
    // Thread monitoring function
    void monitorThreadFunc();
    
    // Pool resizing functions
    void growPool(size_t additionalThreads);
    void shrinkPool(size_t newSize);
    
    // Task structure
    struct Task {
        std::shared_ptr<ClientInfo> clientInfo;
        std::function<void(std::shared_ptr<ClientInfo>)> function;
    };
    
    // Thread pool data members
    std::queue<Task> taskQueue;
    std::vector<std::thread> threads;
    std::thread monitorThread;
    mutable std::mutex queueMutex;
    std::condition_variable taskCondition;
    std::condition_variable spaceCondition;
    
    std::atomic<size_t> currentSize;
    size_t initialSize;
    
    // Thread pool statistics
    struct PoolStats {
        std::atomic<size_t> activeThreads{0};
        std::atomic<size_t> currentPoolSize{0};
        std::chrono::steady_clock::time_point lastResizeTime;
        std::atomic<bool> isMonitoring{true};
    } stats;
    
    std::atomic<bool> stop{false};
    std::atomic<bool> resizeInProgress{false};
    
    // Constants
    static constexpr size_t MAX_THREADS = 64;
    static constexpr size_t MIN_THREADS = 4;
    static constexpr size_t QUEUE_SIZE = 1000;
    static constexpr int UTILIZATION_CHECK_INTERVAL = 10; // seconds
    static constexpr int RESIZE_INTERVAL = 60; // seconds
    static constexpr double UTILIZATION_THRESHOLD = 0.5; // 50%
};

#endif // THREAD_POOL_H