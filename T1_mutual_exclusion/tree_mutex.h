#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <cmath>
#include <assert.h>

class peterson_mutex {

public:
     peterson_mutex() {
         want[0].store(false);
         want[1].store(false);
         victim.store(0);
     }

     void lock(int t) {
         want[t].store(true);
         victim.store(t);
         while (want[1 - t].load() && victim.load() == t) {
             std::this_thread::yield();
         }
     }

     void unlock(int t) {
         want[t].store(false);
     }

private:
    std::array<std::atomic<bool>, 2> want;
    std::atomic<int> victim;
};

class tree_mutex{

public:

    tree_mutex(std::size_t num_threads): tree(getRealSize(num_threads)) {}

    void lock(std::size_t thread_index) {

        int now = tree.size() - pow(2, tree_height) + thread_index;

        while (now > 0){
        	
            if (now % 2 == 0){
            	tree[(now - 2) / 2].lock(0);
            	now = (now - 2) / 2;
            }

            else {
            	tree[(now - 1) / 2].lock(1);
                now = (now - 1) / 2;
            }
        }

    }

    void unlock(std::size_t thread_index) {
        std::vector<int> path;
        int now = tree.size() - pow(2, tree_height) + thread_index;

        while (now >= 0) {
            path.push_back(now);

            if (now % 2 == 0){
                now = (now - 2) / 2;
            }
            else now = (now - 1) / 2;
        }

        for (auto it = path.rbegin(); it < path.rend() -1; it++){
            tree[*it].unlock(*(it + 1) % 2);
       }
    }

private:

    int getRealSize(int num_threads){

        tree_height = ceil(log10(num_threads)/log10(2));

        int real_size = 0;

        for (int i = 0; i <= tree_height; ++i){
            real_size = real_size + pow(2,i);
        }
        return real_size;
    }

    int tree_height;
    std::vector<peterson_mutex> tree;

};
