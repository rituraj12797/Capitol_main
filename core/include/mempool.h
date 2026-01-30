#pragma once
#include <vector>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <new>

// compiler hints for branch prediciton
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

template<typename T>
class MemPool {
private:
    struct ObjectBlock {
        T object;
        // keeping this flag just for safety, but we wont check it in prod
        bool is_occupied = false; 
    };

    // the actual vector where we store stuff
    std::vector<ObjectBlock> store;
    
    // stack to keep track of free indices (lifo is better for cache)
    std::vector<size_t> free_indices; 
    
    // this points to the next fresh block we havent used yet
    size_t high_water_mark = 0;
    size_t capacity;

public:
    explicit MemPool(size_t element_count) : capacity(element_count) {
        store.resize(element_count);
        // reserve space so we dont reallocate during trading
        free_indices.reserve(element_count / 10); 
    }

    // delete these constructors to avoid weird behavior
    MemPool() = delete;
    MemPool(const MemPool&) = delete;
    MemPool& operator=(const MemPool&) = delete;

    // hot path allocator 
    template<typename... Args>
    T* allocate(Args&&... args) noexcept {
        size_t index;

        // 1. check if we have any recycled index in free list
        if (LIKELY(!free_indices.empty())) {
            index = free_indices.back();
            free_indices.pop_back();
        } 
        // 2. if no free index then take fresh memory from high water mark
        else if (LIKELY(high_water_mark < capacity)) {
            index = high_water_mark++; // post increment is fast
        } 
        // 3. crash if no memory
        else {
            // this branch is unlikely to be taken
            std::cerr << "CRITICAL: mempool exhausted!" << std::endl;
            std::terminate();
        }

        ObjectBlock* block = &store[index];
        block->is_occupied = true; // just mark it true fast

        // placement new to construct object here at this address
        T* ptr = &(block->object);
        new(ptr) T(std::forward<Args>(args)...);

        return ptr;
    }

    // hot path deallocator
    void deallocate(T* ptr) noexcept {
        // assume ptr is valid cuz single threaded
        // do pointer arithmetic to find index
        
        ObjectBlock* block_ptr = reinterpret_cast<ObjectBlock*>(ptr);
        ObjectBlock* base_ptr = &store[0];
        
        // this calculation is very fast single instruction
        size_t index = block_ptr - base_ptr;

        // no checks just do it
        store[index].is_occupied = false;
        
        // push to free list so we can resue it later
        free_indices.push_back(index);
    }
};


/*
 ===============================================================================
 strategy explanation
 ===============================================================================
 
 so here is the stroy of how we optimized this memory pool.

 1. the naive approach (linear scan):
 initially, we just had a vector of objects with an `is_occupied` flag. whenever 
 we needed to allocate memory, we woudl loop through the entire vector linearly 
 to find a free spot. this was terrible because as the pool got full, the scan 
 took longer and longer (o(n)), making our latency unpredictable.

 2. the free list approach:
 to fix the scan, we added a 'free list' (a stack of indices). when we deleted 
 an object, we pushed its index to the stack. when allocating, we popped from 
 the stack. this made it o(1). but then we hit a new problem: if we wanted 
 50 million objects, we had to push 50 million indices into the stack at 
 startup. this killed our startup time and wasted huge amounts of cpu cache 
 storing numbers we didn't need yet.

 3. the hybrid strategy (current):
 we decided to combine the best of both. we use a 'high water mark' (a simple 
 counter) AND a 'free list'. 
 - at startup, the free list is empty. 
 - when we need new memory, we just increment the high water mark (instant). 
 - only when we free something do we push it to the free list.
 
 for allocation, we check the free list first (recycling warm memory). if its 
 empty, we just increment the high water mark. this gives us instant startup 
 time and guaranteed o(1) speed forever.

 ===============================================================================
 visual representation
 ===============================================================================

      [ memory store (vector) ]
      _______________________________________________________
     | used | used | free | used | used | fresh | fresh | ...
     |__0___|__1___|__2___|__3___|__4___|__5____|__6____|___
                      ^                    ^
                      |                    |__ high water mark
            (recycled index 2)             (points to never-used memory)
                      |
                      |
      [ free list (stack) ]
      _________________
     |       2         |  <-- we check here first! (hot cache)
     |_________________|      if empty, we use the high water mark.

    Once the high water mark reaches end, then we wont be able to use it and till then due to some deallocation (which probably will happen) free list will have free entries, 
    why use stack here and why not queue ?? == >the last index which was fred, migh be sitting in cache so it's betetr to retrieve that instead of calling the last entry, which is least likely to be in cache.

*/