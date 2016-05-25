#include <iostream>
#include <mutex>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <forward_list>
#include <algorithm>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <shared_mutex>

class RW_mutex {
public:
   RW_mutex() : writers(0), readers(0), writing(false) {}

    void write_lock(){
        std::unique_lock<std::mutex> lock(gate);
        ++writers;
        while (writing || readers > 0){
           room_empty.wait(lock);
        }
        writing = true;
    }

    void read_lock(){
        std::unique_lock<std::mutex> lock(gate);
        while (writers > 0){
            room_empty.wait(lock);
        }
        ++readers;
    }

    void write_unlock(){
        std::unique_lock<std::mutex> lock(gate);
        --writers;
        writing = false;
        lock.unlock();
        room_empty.notify_all();
    }

    void read_unlock(){
        std::unique_lock<std::mutex> lock(gate);
        --readers;
        if (readers == 0) 
            room_empty.notify_all();
    }

private:
   std::mutex gate;
   std::size_t writers;
   std::size_t readers;
   std::condition_variable room_empty;
   bool writing;
};

template <typename Value, class Hash = std::hash<Value>>
class striped_hash_set {
public:

   striped_hash_set () : num_stripes(5), growth_factor(2), lock_striping(5), amount(0){
        table.resize(50);
   }


   explicit striped_hash_set (std::size_t num_stripes_, std::size_t growth_factor_ = 2, std::size_t max_load_factor = 4) :
                                          num_stripes(num_stripes_), growth_factor(growth_factor_),
                                          lock_striping(num_stripes), amount(0), max_load_factor(max_load_factor) {

        table.resize(num_stripes * 35);
    }


    void remove (const Value& e){
        std::size_t stripe_index_ = stripe_index(e);

        lock_striping[stripe_index_].write_lock();

        std::size_t table_index_ = table_index(e);
        if (belonging(e)){
            auto it = std::find(table[table_index_].begin(), table[table_index_].end(), e);
            table[table_index_].remove(*it);
            amount--;
         }

        lock_striping[stripe_index_].write_unlock();
    }

    void add (const Value& e) {
        std::size_t stripe_index_ = stripe_index(e);

        lock_striping[stripe_index_].write_lock();
        if (!belonging(e)) {
            table[table_index(e)].push_front(e);
            amount++;
        }

        if (get_load_factor() > max_load_factor) {
            lock_striping[stripe_index_].write_unlock();
            resize();
        }

        else lock_striping[stripe_index_].write_unlock();
    }

    bool contains (const Value& e) {
        std::size_t stripe_index_ = stripe_index(e);

        lock_striping[stripe_index_].read_lock();
        
        bool result = belonging(e);

        lock_striping[stripe_index_].read_unlock();
        return result;
    }

    void resize () {

        // lock all mutex
        for (std::size_t i = 0; i < num_stripes; ++i) {
            lock_striping[i].write_lock();
        }

        if (get_load_factor() > max_load_factor) {

            // resize
            std::vector<std::forward_list<Value>> tmp_table(table.size() * growth_factor);

            // rehashing
            for (std::size_t i = 0; i < table.size(); ++i){
                for (auto it = table[i].begin(); it != table[i].end(); ++it){
                    std::size_t table_index_ = hash_function(*it) % tmp_table.size();
                    tmp_table[table_index_].push_front(*it);
                }
            }

            table = std::move(tmp_table);
        }

        for (std::size_t i = 0; i < num_stripes; ++i) {
            lock_striping[i].write_unlock();
        }
    }

private:

    std::size_t get_load_factor () {
        return amount / table.size();
    }

    std::size_t table_index (const Value& e) {
        return hash_function(e) % table.size();
    }

    std::size_t stripe_index (const Value& e) {
        return hash_function(e) % num_stripes;
    }

    bool belonging (const Value& e) {
        std::size_t table_index_ = table_index(e);
        return std::find(table[table_index_].begin(), table[table_index_].end(), e) != table[table_index_].end();
    }

    std::size_t num_stripes;
    std::size_t growth_factor;
    std::vector<RW_mutex> lock_striping;
    std::vector<std::forward_list<Value>> table;
    std::atomic<std::size_t> amount;
    std::size_t max_load_factor;
    Hash hash_function;
};
