#include <iostream>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <vector>
#include <cstdlib>
#include <unistd.h>
#include <atomic>

class barrier {

public:
    explicit barrier(size_t num_threads) : num_threads(num_threads), count_in(0), count_out(0) {}

    void enter(){

        std::unique_lock<std::mutex> lock(current_mutex);

        while (count_out > 0){
            door_in.wait(lock);
        }

        door_in.notify_all();

        count_in.fetch_add(1);

        if (count_in == num_threads){
             count_out.fetch_add(num_threads);
        }

        while (count_in != num_threads){
            door_out.wait(lock);
        }

        door_out.notify_all();

        count_out.fetch_sub(1);

        if (count_out == 0){
             count_in.fetch_sub(num_threads);
        }
    }


private:
    std::size_t num_threads;
    std::mutex current_mutex;
    std::atomic<std::size_t> count_in;
    std::atomic<std::size_t> count_out;
    std::condition_variable door_in;
    std::condition_variable door_out;
};
