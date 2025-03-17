
#ifndef NETPROBE_H
#define NETPROBE_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <queue>
#include <chrono>
#include <functional>
#define infinite 1000000000
#define MAX_THREADS 64
#define MIN_THREADS 4
#define DEFAULT_INITIAL_THREADS 8
#define QUEUE_SIZE 1000
#define UTILIZATION_CHECK_INTERVAL 10  // seconds
#define RESIZE_INTERVAL 60  // seconds
#define UTILIZATION_THRESHOLD 0.5  // 50%
using namespace std;
class Net_opt {
    public:
        enum mode {SEND, RECV,RESPONSE} mode;
        int stat = 500; // Default 500 ms
        string rhost = "localhost"; // Default localhost
        string lhost = "IN_ADDR_ANY"; // Default Late Binding
        int rport = 4180, lport = 4180; // Default 4180
        string proto = "UDP"; // Default UDP
        int pktsize = 1000; // Default 1000 bytes
        int pktrate = 1000; // Default 1000 bytes/second
        int pktnum = infinite; // Default 0 (infinite)
        int sbufsize = 0;
        int rbufsize = 0;
        string isPresistent = "No"; // Default No
        string tcpcca;
        int poolsize = 8; // Default 8
};

class ClientInfo {
    public:
        int socket;
        struct sockaddr_in addr;
        int msg[7]; //0: mode, 1: stat, 2: proto, 3: pktsize, 4: pktrate, 5: pktnum, 6: isPresistent
        int snr_buf[2];
        string host;
        int port;
};

struct Task {
    ClientInfo* client_info;
    std::function<void(ClientInfo*)> function;
};

typedef struct {
    atomic<size_t> active_threads;
    atomic<size_t> current_pool_size;
    chrono::steady_clock::time_point last_resize_time;
    atomic<bool> is_monitoring;
} PoolStats;

class ThreadPool {
    private:
        queue<Task> task_queue;
        vector<thread> threads;
        thread monitor_thread;
        mutex queue_mutex;
        condition_variable task_cond;
        condition_variable space_cond;
        
        atomic<size_t> current_size;
        size_t initial_size;
        
        PoolStats stats;
        atomic<bool> stop;
        atomic<bool> resize_in_progress;

    public:
        ThreadPool(size_t initial_threads = DEFAULT_INITIAL_THREADS) : 
        current_size(initial_threads), initial_size(initial_threads), stop(false), resize_in_progress(false) {
        
        stats.active_threads = 0;
        stats.current_pool_size = current_size.load();
        stats.is_monitoring = true;
        stats.last_resize_time = std::chrono::steady_clock::now();


        for (size_t i = 0; i < initial_threads; i++) {
            threads.emplace_back(&ThreadPool::worker_thread, this);
        }

        monitor_thread = thread(&ThreadPool::monitor_thread_func, this);
        }
    
    int get_active_threads() {
            return stats.active_threads.load();
        }
    int get_current_pool_size() {
            return stats.current_pool_size.load();
        }

    ~ThreadPool() {
        stats.is_monitoring = false;

        task_cond.notify_all();

        if (monitor_thread.joinable()) {
            monitor_thread.join();
        }

        stop = true;
        task_cond.notify_all();

        for (std::thread& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    bool enqueue(ClientInfo* client_info, std::function<void(ClientInfo*)> function) {
        unique_lock<mutex> lock(queue_mutex);

        while (task_queue.size() >= QUEUE_SIZE && !stop) {
            size_t current = current_size.load();
            if (current < MAX_THREADS && !resize_in_progress) {
                size_t new_size = std::min(current * 2, (size_t)MAX_THREADS);
                grow_pool(new_size - current);
            }
            space_cond.wait(lock);
        }

        if (stop) {
            return false;
        }

        task_queue.push({client_info, function});
        task_cond.notify_one();
        return true;
    }
    private:
    void worker_thread() {
        while (!stop) {
            Task task;
            bool have_task = false;
            
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                while (task_queue.empty() && !stop) {
                    task_cond.wait(lock);
                }

                if (!task_queue.empty()) {
                    task = task_queue.front();
                    task_queue.pop();
                    have_task = true;
                    space_cond.notify_one();
                }
            }

            if (have_task) {
                stats.active_threads++;
                task.function(task.client_info);
                stats.active_threads--;
            }
        }
    }

    void monitor_thread_func() {
        while (stats.is_monitoring) { 
        
            // Use a condition variable with timeout
            unique_lock<mutex> sleep_lock(queue_mutex);
            bool timeout = !task_cond.wait_for(sleep_lock, 
                std::chrono::seconds(UTILIZATION_CHECK_INTERVAL),
                [this]() { return !stats.is_monitoring; });
            
            // If no longer monitoring, exit immediately
            if (!stats.is_monitoring) {
                puts("monitoring stopped during sleep");
                break;
            }
            sleep_lock.unlock();

            auto current_time = std::chrono::steady_clock::now();
            auto time_since_resize = std::chrono::duration_cast<std::chrono::seconds>(
                current_time - stats.last_resize_time).count();
            if (time_since_resize < RESIZE_INTERVAL) {
                continue;
            }
            
            size_t active = stats.active_threads.load();
            size_t current = current_size.load();
            double utilization = static_cast<double>(active) / current;

            unique_lock<mutex> resize_lock(queue_mutex);
            if (utilization < UTILIZATION_THRESHOLD && current > MIN_THREADS && 
                !resize_in_progress) {
                size_t new_size = max(current / 2, (size_t)MIN_THREADS);
                resize_lock.unlock();
                shrink_pool(new_size);
            }
        }
    }

    void grow_pool(size_t additional_threads) {
        resize_in_progress = true;
        size_t current = current_size.load();
        size_t new_size = current + additional_threads;
        cout << "Pool size increased to " << new_size << endl;

        for (size_t i = current; i < new_size; i++) {
            threads.emplace_back(&ThreadPool::worker_thread, this);
        }

        current_size = new_size;
        stats.last_resize_time = std::chrono::steady_clock::now();
        resize_in_progress = false;
        
    }

    void shrink_pool(size_t new_size) {
        resize_in_progress = true;
        size_t current = current_size.load();
        size_t threads_to_remove = current - new_size;
        cout << "Pool size decreased to " << threads_to_remove << endl;

        stop = true;
        task_cond.notify_all();

        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        threads.clear();
        stop = false;

        for (size_t i = 0; i < new_size; i++) {
            threads.emplace_back(&ThreadPool::worker_thread, this);
        }

        current_size = new_size;
        stats.last_resize_time = std::chrono::steady_clock::now();
        resize_in_progress = false;
        
    }
};

#endif // NETPROBE_H


