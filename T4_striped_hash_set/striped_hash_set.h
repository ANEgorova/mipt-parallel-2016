#include <iostream>
#include <mutex>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <forward_list>
#include <algorithm>
#include <condition_variable>
#include <atomic>

class RW_mutex{

public:
    RW_mutex() : writers(0), readers(0), writing(false) {}

    void write_lock(){
        std::unique_lock<std::mutex> lock(gate);
        writers++;
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
        readers++;
    }

    void write_unlock(){
        std::unique_lock<std::mutex> lock(gate);
        writers--;
        writing = false;
        lock.unlock();
        room_empty.notify_all();
    }

    void read_unlock(){
        std::unique_lock<std::mutex> lock(gate, std::defer_lock);
        lock.lock();
        readers--;
        if (readers == 0) room_empty.notify_one();
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

   striped_hash_set () : num_stripes(5), table_size(50), growth_factor(2), load_factor(0), lock_striping(5){
        table.resize(table_size);
   }


   explicit striped_hash_set(std::size_t num_stripes_, std::size_t growth_factor_ = 2, double coefficient = 1) :
                                          num_stripes(num_stripes_),
                                          table_size(num_stripes * 35), growth_factor(growth_factor_),
                                          load_factor(0), lock_striping(num_stripes), coefficient(coefficient) {

        table.resize(table_size);
    }


    void remove(const Value& e){

        lock_striping[stripe_index(e)].write_lock();
        if (belonging(e)){
            auto it = std::find(table[table_index(e)].begin(), table[table_index(e)].end(), e);
            table[table_index(e)].remove(*it);
            amount--;
         }

        lock_striping[stripe_index(e)].write_unlock();
    }

    void add(const Value& e) {
       lock_striping[stripe_index(e)].write_lock();

       if (!belonging(e)) {
            table[table_index(e)].push_front(e);
            amount++;
       }

       if (load_factor > coefficient) {
          lock_striping[stripe_index(e)].write_unlock();
          resize();
       }
       else lock_striping[stripe_index(e)].write_unlock();
    }

    bool contains(const Value& e) {
        lock_striping[stripe_index(e)].read_lock();
        bool result;

        result = belonging(e);

        lock_striping[stripe_index(e)].read_unlock();
        return result;
    }

    std::size_t set_load_factor(){
        load_factor =  amount / table_size;
        return load_factor;
    }

    std::size_t table_index (const Value& e){
        return hash_function(e) % table_size;
    }

    std::size_t stripe_index(const Value& e){
        return table_index(e) % num_stripes;
    }

    void resize() {

        if (amount > table_size * coefficient){
            // resize
            table_size.fetch_add(table_size*(growth_factor-1));
            set_load_factor();

            // lock all mutex
            for (std::size_t i = 0; i < num_stripes; ++i) {
                lock_striping[i].write_lock();
            }

            std::vector<std::forward_list<Value>> tmp_table(table_size);
            // rehashing
            for (std::size_t i = 0; i < table_size / growth_factor; ++i){
                for (auto it = table[i].begin(); it != table[i].end(); ++it){
                    tmp_table[table_index(*it)].push_front(*it);
            }

            table = tmp_table;
            }

            for (std::size_t i = 0; i < num_stripes; ++i) {
                lock_striping[i].write_unlock();
            }
       }
    }

    bool belonging(const Value& e) {

        bool condition = std::find(table[table_index(e)].begin(), table[table_index(e)].end(), e) != table[table_index(e)].end();

        if (condition) return true;
        else return false;
    }

private:
    std::size_t num_stripes;
    std::atomic<std::size_t> table_size;
    std::size_t growth_factor;
    double load_factor;
    std::vector<RW_mutex> lock_striping;
    std::vector<std::forward_list<Value>> table;
    std::atomic<std::size_t> amount;
    std::size_t coefficient;
    Hash hash_function;
};