#include <atomic>
#include <vector>
#include <thread>

template <typename Value>
class spsc_ring_buffer {
    
public:
    
    spsc_ring_buffer(const size_t capacity_) : buf(capacity_ + 1), head(0), tail(0), capacity(capacity_ + 1) {}
    
    bool enqueue(const Value& value) {
        
        size_t tail_ = tail.load(std::memory_order_relaxed);
        size_t after_tail = next(tail);
        if (after_tail == head.load(std::memory_order_acquire)) // переполнение
            return false;
        buf[tail_] = value;
        tail.store(after_tail, std::memory_order_release);
        return true;
    }
    
    // Обоснование:
    // У нас есть чтения и запись атомарных переменных в разных потоках (head, tail)
    // Тогда нужно расставить release и aquire там, где необходимо видеть записи в память из других потоков, то есть 
    // организовать SW. Тогда нам нужно написать их, чтобы поддерживать актуальные значения head и tail(для корректной работы), а, значит,
    // поставить там, где поставила. 
    
    // В случае первых считываний head и tail нужно использовать relaxed, потому что мы сами записали последними это значение
    // и видим его (т.к. по условию 1 producer и 1 consumer).
    
    bool dequeue(Value& value) {
        
        size_t head_ = head.load(std::memory_order_relaxed);
        if (head_ == tail.load(std::memory_order_acquire)) // пустота
            return false; 
        value = buf[head_];
        head.store(next(head_), std::memory_order_release);
        return true;
    }
    
private:
     
    std::vector<Value> buf;
    std::atomic<size_t> head;
    std::atomic<size_t> tail;
    size_t capacity;
    
    size_t next (size_t curr) {
        return (curr + 1) % capacity;
    }
};

