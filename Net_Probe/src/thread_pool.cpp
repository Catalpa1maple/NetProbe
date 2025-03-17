#include "../include/thread_pool.h"
#include <iostream>
#include <thread>
#include <chrono>

// Initialize mutex for thread-safe output
std::mutex outputMutex;

ThreadPool::ThreadPool(size_t initialThreads) 
    : currentSize(initialThreads), initialSize(initialThreads), stop(false), resizeInProgress(false) {
    
    stats.activeThreads = 0;
    stats.currentPoolSize = currentSize.load();
    stats.isMonitoring = true;
    stats.lastResizeTime = std::chrono::steady_clock::now();

    // Create initial worker threads
    for (size_t i = 0; i < initialThreads; i++) {
        threads.emplace_back(&ThreadPool::workerThread, this);
    }

    // Start monitoring thread
    monitorThread = std::thread(&ThreadPool::monitorThreadFunc, this);
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        stats.isMonitoring = false;
    }
    
    // Stop monitoring thread
    taskCondition.notify_all();
    if (monitorThread.joinable()) {
        monitorThread.join();
    }

    // Stop worker threads
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        stop = true;
    }
    
    taskCondition.notify_all();

    // Join all worker threads
    for (std::thread& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

bool ThreadPool::enqueue(std::shared_ptr<ClientInfo> clientInfo, 
                         std::function<void(std::shared_ptr<ClientInfo>)> function) {
    std::unique_lock<std::mutex> lock(queueMutex);

    // Wait for space in the queue if it's full
    while (taskQueue.size() >= QUEUE_SIZE && !stop) {
        size_t current = currentSize.load();
        if (current < MAX_THREADS && !resizeInProgress) {
            size_t newSize = std::min(current * 2, static_cast<size_t>(MAX_THREADS));
            lock.unlock();
            growPool(newSize - current);
            lock.lock();
        } else {
            spaceCondition.wait(lock);
        }
    }

    if (stop) {
        return false;
    }

    // Add task to queue
    taskQueue.push({clientInfo, function});
    lock.unlock();
    
    // Notify one worker thread
    taskCondition.notify_one();
    return true;
}

size_t ThreadPool::getActiveThreads() const {
    return stats.activeThreads.load();
}

size_t ThreadPool::getPoolSize() const {
    return stats.currentPoolSize.load();
}

void ThreadPool::workerThread() {
    while (!stop) {
        Task task;
        bool haveTask = false;
        
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            while (taskQueue.empty() && !stop) {
                taskCondition.wait(lock);
            }

            if (!taskQueue.empty()) {
                task = taskQueue.front();
                taskQueue.pop();
                haveTask = true;
                spaceCondition.notify_one();
            }
        }

        if (haveTask) {
            stats.activeThreads++;
            try {
                task.function(task.clientInfo);
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(outputMutex);
                std::cerr << "Exception in worker thread: " << e.what() << std::endl;
            } catch (...) {
                std::lock_guard<std::mutex> lock(outputMutex);
                std::cerr << "Unknown exception in worker thread" << std::endl;
            }
            stats.activeThreads--;
        }
    }
}

void ThreadPool::monitorThreadFunc() {
    while (stats.isMonitoring) {
        // Use a condition variable with timeout for periodic monitoring
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            taskCondition.wait_for(lock, 
                std::chrono::seconds(UTILIZATION_CHECK_INTERVAL),
                [this]() { return !stats.isMonitoring; });
            
            if (!stats.isMonitoring) {
                break;
            }
        }

        auto currentTime = std::chrono::steady_clock::now();
        auto timeSinceResize = std::chrono::duration_cast<std::chrono::seconds>(
            currentTime - stats.lastResizeTime).count();
            
        if (timeSinceResize < RESIZE_INTERVAL) {
            continue;
        }
        
        size_t active = stats.activeThreads.load();
        size_t current = currentSize.load();
        double utilization = static_cast<double>(active) / current;

        if (utilization < UTILIZATION_THRESHOLD && current > MIN_THREADS && 
            !resizeInProgress) {
            size_t newSize = std::max(current / 2, static_cast<size_t>(MIN_THREADS));
            shrinkPool(newSize);
        }
    }
}

void ThreadPool::growPool(size_t additionalThreads) {
    resizeInProgress = true;
    size_t current = currentSize.load();
    size_t newSize = current + additionalThreads;
    
    {
        std::lock_guard<std::mutex> lock(outputMutex);
        std::cout << "Growing thread pool from " << current << " to " << newSize << std::endl;
    }

    for (size_t i = 0; i < additionalThreads; i++) {
        threads.emplace_back(&ThreadPool::workerThread, this);
    }

    currentSize = newSize;
    stats.currentPoolSize = newSize;
    stats.lastResizeTime = std::chrono::steady_clock::now();
    resizeInProgress = false;
}

void ThreadPool::shrinkPool(size_t newSize) {
    if (newSize >= currentSize.load()) {
        return;
    }

    resizeInProgress = true;
    
    {
        std::lock_guard<std::mutex> lock(outputMutex);
        std::cout << "Shrinking thread pool from " << currentSize.load() << " to " << newSize << std::endl;
    }

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        stop = true;
    }
    
    taskCondition.notify_all();

    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    threads.clear();
    stop = false;

    for (size_t i = 0; i < newSize; i++) {
        threads.emplace_back(&ThreadPool::workerThread, this);
    }

    currentSize = newSize;
    stats.currentPoolSize = newSize;
    stats.lastResizeTime = std::chrono::steady_clock::now();
    resizeInProgress = false;
}