#include <iostream>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <deque>
#include <vector>
#include <cstdlib>
#include <future>

template <class T>
class thread_safe_queue {

public:
    thread_safe_queue(std::size_t capacity) {
        this->capacity = capacity;
    }

    thread_safe_queue(const thread_safe_queue &obj) = delete;

    bool enqueue(T& item){
        std::unique_lock<std::mutex> my_lock(some_mutex);
        if (flag){
            return false;
        }
        safe_queue.push_back(std::move(item));
        cv_deq.notify_one();
        return true;
    }

    bool pop(T& item){
        std::unique_lock<std::mutex> my_lock(some_mutex);

        while(safe_queue.empty() && !flag){
            cv_deq.wait(my_lock);
        }
        if (flag && safe_queue.empty()){
            return false;
        }
        item = std::move(safe_queue.front());
        safe_queue.pop_front();
        cv_enq.notify_all();
        return true;
    }

    void shutdown(){
        this->some_mutex.lock();
        flag = true;
        this->some_mutex.unlock();
        cv_enq.notify_all();
        cv_deq.notify_all();
    }


private:
    std::size_t capacity;
    std::condition_variable cv_enq;
    std::condition_variable cv_deq;
    std::mutex some_mutex;
    std::deque<T> safe_queue;
    bool flag = false;
};


template <class R>
class thread_pool {
public:
    thread_pool() : queue(0) {
        this->num_threads = default_num_workers();
        this->threads.resize(num_threads);
        create_threads();
    }

    thread_pool(std::size_t num_threads) : num_threads(num_threads), queue(0){
        this->threads.resize(this->num_threads);
        create_threads();
    }

    ~thread_pool(){
        this->shutdown();
    }

    std::future<R> submit (std::function<R()> func){
        std::packaged_task<R(void)> sometask(func);
        std::future<R> result = sometask.get_future();
        queue.enqueue(sometask);
        return result;
    }

    void shutdown(){
        this->queue.shutdown();

        for (std::size_t i = 0; i < num_threads; i++){
            this->threads[i].join();
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
    std::vector<std::thread> threads;
    thread_safe_queue<std::packaged_task<R(void)>> queue;
    void create_threads(){
        for (std::size_t i = 0; i < this->num_threads; i++){
            this->threads[i] = std::thread(doing_task, std::ref(queue));
        }
    }

    static void doing_task(thread_safe_queue<std::packaged_task<R(void)>> & queue) {
        std::packaged_task<R(void)> item;
        while(queue.pop(item)){
            item();
        }
    }
};


// int f(int x, int y){
//     return x + y;
// }

// int d(){
//     return f(1, 1);
// }
// int main(){

//     thread_pool<int> A(4);
//     std::function<int(void)> task1 = d;
//     std::cout << A.submit(task1).get() << std::endl;
// }
