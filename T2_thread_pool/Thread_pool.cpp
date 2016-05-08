#include <iostream>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <deque>
#include <vector>
#include <cstdlib>
#include <future>
#include <exception>
#include <algorithm>
#include <atomic>

template <class T>
class thread_safe_queue {

public:
    explicit thread_safe_queue(std::size_t capacity_) : shutdown_flag(false) {
        capacity = capacity_;
    }

    thread_safe_queue(const thread_safe_queue& obj) = delete;

    bool enqueue(T item){
        std::unique_lock<std::mutex> lock(mutex);

        if (shutdown_flag){
            throw std::runtime_error("Shutdown was called");
        }

        safe_queue.push_back(std::move(item));
        cv_deq.notify_one();
        return true;
    }

    bool pop(T& item){
        std::unique_lock<std::mutex> lock(mutex);

        while(safe_queue.empty() && !shutdown_flag){
            cv_deq.wait(lock);
        }

        if (shutdown_flag && safe_queue.empty()){
            return false;
        }

        item = std::move(safe_queue.front());
        safe_queue.pop_front();
        cv_enq.notify_one();
        return true;
    }

    void shutdown(){
        std::unique_lock<std::mutex> lock(mutex);
        if (shutdown_flag){
            throw std::runtime_error("Repeted shutdown call");
        }
        shutdown_flag = true;
        lock.unlock();
        cv_enq.notify_all();
        cv_deq.notify_all();
    }


private:
    std::size_t capacity;
    std::condition_variable cv_enq;
    std::condition_variable cv_deq;
    std::mutex mutex;
    std::deque<T> safe_queue;
    std::atomic<bool> shutdown_flag;
};


template <class Value>
class thread_pool {
public:

    explicit thread_pool(std::size_t num_threads_) : num_threads(num_threads_), queue(0), shutdown_flag(false), threads(num_threads_){
        create_threads();
    }

    explicit thread_pool() : queue(0), shutdown_flag(false), threads(num_threads) {
        num_threads = default_num_workers();
        create_threads();
    }

    ~thread_pool() {

        if (!shutdown_flag){
            shutdown();
        }
    }

    std::future<Value> submit (std::function<Value()> function_){
        std::packaged_task<Value(void)> current_task(function_);
        std::future<Value> result = current_task.get_future();
        queue.enqueue(std::move(current_task));
        return result;
    }

    void shutdown() {
        queue.shutdown();
        shutdown_flag = true;
        for (std::size_t i = 0; i < num_threads; i++){
            threads[i].join();
        }

    }

    std::size_t default_num_workers() {

        if (!std::thread::hardware_concurrency())
            return 2;
        else
            return std::thread::hardware_concurrency();
    }

private:
    std::size_t num_threads;
    thread_safe_queue<std::packaged_task<Value(void)>> queue;
    std::atomic<bool> shutdown_flag;
    std::vector<std::thread> threads;

    void create_threads(){
        for (std::size_t i = 0; i < this->num_threads; i++){
            threads[i] = std::thread(doing_task, std::ref(queue));
        }
    }

    static void doing_task(thread_safe_queue<std::packaged_task<Value(void)>>& queue) {
        std::packaged_task<Value(void)> item;
        while(queue.pop(item)){
            item();
        }
    }
};