#include <iostream>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <deque>
#include <vector>
#include <cstdlib>
#include <unistd.h>

template <class T>
class thread_safe_queue{

public:
    thread_safe_queue(std::size_t capacity) {
        this->capacity = capacity;
    }

    thread_safe_queue(const thread_safe_queue &obj) = delete;

    bool enqueue(T item){
        std::unique_lock<std::mutex> my_lock(some_mutex);
        while((safe_queue.size() >= this->capacity) && !flag){
            cv_enq.wait(my_lock);
        }
        if (flag){
            return false;
        }
        std::cout << "Insert: " << item << std::endl;
        safe_queue.push_back(item);
        std::cout << "After unlock in insert" << std::endl;
        cv_deq.notify_one();
        return true;
    }

    bool pop(T& item){
        std::cout << "Here" << std::endl;
        std::unique_lock<std::mutex> my_lock(some_mutex);

        while(safe_queue.empty() && !flag){
            cv_deq.wait(my_lock);
        }
        std::cout << "Ready" << std::endl;
        std::cout << "After lock" << std::endl;
        if (flag && safe_queue.empty()){
            return false;
        }
        item = safe_queue.front();
        std::cout << "Pop: "<< item << std::endl;
        safe_queue.pop_front();
        cv_enq.notify_one();
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

thread_safe_queue<int> my_queue(10);

void generate (int size){
    srand (time(NULL));
    for (int i = 0; i < size; ++i){
        my_queue.enqueue(rand() % 100);
    }
    my_queue.shutdown();
}

void prime (){
    int x = 0;
    while (my_queue.pop(x)){
        for (int i = 2; i <= x / 2; ++i){
            if (x % i == 0){
                break;
            }
        }
    }
}

void f1() {
    std::cout << "f1" << std::endl;
    int x;
    my_queue.pop(x);
    std::cout << x << std::endl;
}

void f2() {
    int x = 1;
    my_queue.enqueue(x);
    std::cout << "END\n";
}

int main(){/*
    std::thread producer(generate, 20);
    std::vector<std::thread> consumers(3);
    for (int i = 0; i < 3; i++){
        consumers[i] = std::thread(prime);
    }

    producer.join();
    for (int i = 0; i < 3; ++i){
        consumers[i].join();
    }*/
    std::thread t(f1);
    sleep(1);
    std::cout << "Wake up" << std::endl;
    std::thread(f2).detach();
    t.join();
}
