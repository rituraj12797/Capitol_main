#pragma once

#include <iostream>
#include <vector>
#include <immintrin.h>
#include <stdexcept>
#include <cstring>
#include <cstdint>
#include <utility>
#include <new>
#include "mempool.h" 

// compiler hints for branch prediction
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

namespace internal_lib {

    // m = 256 means 256 keys, 257 children. 
    
    template <typename KeyType = long long, typename ValueType = int, int M = 256>
    class SIMDBPlusTree {
    private:
        // forward declaration to define mempool
        struct Node;

        //  This vector holds nodes, and the pointer in our B+ Tree is nothing but the location of these nodes, these nodes live inside the Mempool.

        using NodePool = MemPool<Node>;
        
        // static pool shared by all trees
        inline static NodePool* pool = nullptr;

        // node aligned to cache line 64 bytes
        struct alignas(64) Node {
            bool is_leaf;
            uint16_t num_keys;
            
            // 256 * 8 = 2048 bytes
            KeyType keys[M]; 

            union {
                // 256 * 4 = 1024 bytes (leaf)
                ValueType values[M];      
                // 257 * 8 = 2056 bytes (internal)
                Node* children[M + 1];    
            };

            // default constructor for safety. 
            Node() : is_leaf(false), num_keys(0) {}
            Node(bool leaf) : is_leaf(leaf), num_keys(0) {}
        };

        Node* root;

        //factory method
        // instead of using constructor we use a function to create the object itself --> factory pattern
        // this function does exzctly this it creates the object inside our memory pool and then gives us back the pointer.

        Node* createNode(bool is_leaf) {
            return pool->allocate(is_leaf);
        }

        // helper to delete tree recursively (if needed)
        void deleteTree(Node* node) {
            if (!node->is_leaf) {
                for (int i = 0; i <= node->num_keys; i++) {
                    deleteTree(node->children[i]);
                }
            }
            pool->deallocate(node);
        }

        //   full simd search (no scalar loop)  
        int findIndexSIMD(Node* node, KeyType key) {
            // broadcast key to 4 lanes (avx2)
            __m256i target = _mm256_set1_epi64x(key);
            
            // round up loop count
            // if num_keys is 5, we loop 2 times (indices 0..3 and 4..7)
            // this is safe because keys[] is size 256. reading garbage at 6,7 is fine.
            int limit = (node->num_keys + 3) / 4; 
            
            // stride by 4
            for (int i = 0; i < limit * 4; i += 4) {
                
                // safe over-read load
                __m256i chunk = _mm256_loadu_si256((__m256i*)&node->keys[i]);
                
                // compare
                __m256i cmp = _mm256_cmpeq_epi64(chunk, target);
                int mask = _mm256_movemask_pd(_mm256_castsi256_pd(cmp));
                
                // if match found
                if (UNLIKELY(mask != 0)) {
                    int bit_pos = __builtin_ctz(mask); // count trailing zeros
                    int found_idx = i + bit_pos;
                    
                    // bounds check: ignore matches in the garbage zone
                    if (found_idx < node->num_keys) {
                        return found_idx;
                    }
                    return -1; // match was in garbage
                }
            }
            
            return -1;
        }

        //   insert logic  
        void insert_recursive(Node* node, KeyType key, ValueType value, Node*& new_sibling, KeyType& median) {
            int i = 0;
            // keeping scalar linear scan for insertion index finding 
            // because we need 'greater than' logic which is expensive in avx2-64bit
            // and we only insert once per order, but we search many times.
            while (i < node->num_keys && key > node->keys[i]) i++;

            if (node->is_leaf) {
                // update existing val ??
                if (i > 0 && node->keys[i - 1] == key) {
                    node->values[i - 1] = value;
                    return;
                }
                
                // shift elements to right to make space
                for (int k = node->num_keys; k > i; k--) {
                    node->keys[k] = node->keys[k - 1];
                    node->values[k] = node->values[k - 1];
                }
                
                node->keys[i] = key;
                node->values[i] = value;
                node->num_keys++;
                
                // split if full
                if (node->num_keys >= M) split_leaf(node, new_sibling, median);
                return;
            }

            // internal node logic
            Node* child_sibling = nullptr;
            KeyType child_median = 0;
            insert_recursive(node->children[i], key, value, child_sibling, child_median);

            if (child_sibling) {
                // shift keys and children to make space
                for (int k = node->num_keys; k > i; k--) node->keys[k] = node->keys[k - 1];
                for (int k = node->num_keys + 1; k > i + 1; k--) node->children[k] = node->children[k - 1];
                
                node->keys[i] = child_median;
                node->children[i + 1] = child_sibling;
                node->num_keys++;

                if (node->num_keys >= M) split_internal(node, new_sibling, median);
            }
        }

        void split_leaf(Node* node, Node*& new_leaf, KeyType& median) {
            int mid = M / 2;
            new_leaf = createNode(true); // use our factory
            
            int num_moving = node->num_keys - mid;
            
            // move half elements to new leaf
            for (int i = 0; i < num_moving; i++) {
                new_leaf->keys[i] = node->keys[mid + i];
                new_leaf->values[i] = node->values[mid + i];
            }
            node->num_keys = mid;
            new_leaf->num_keys = num_moving;
            median = new_leaf->keys[0]; // copy up median
        }

        void split_internal(Node* node, Node*& new_node, KeyType& median) {
            int mid = M / 2;
            new_node = createNode(false); // use our factory
            
            median = node->keys[mid]; // move median up
            int num_moving = node->num_keys - (mid + 1);

            for (int i = 0; i < num_moving; i++) new_node->keys[i] = node->keys[mid + 1 + i];
            for (int i = 0; i <= num_moving; i++) new_node->children[i] = node->children[mid + 1 + i];
            
            node->num_keys = mid;
            new_node->num_keys = num_moving;
        }

    public:
        // initializer  --> the main function must call this before hot path starts. 
        // user MUST call this in main() before creating any tree
        static void init(size_t pool_size) {
            if (!pool) {
                pool = new NodePool(pool_size);
            }
        }

        SIMDBPlusTree() { 
            // fallback safety: if user forgot init(), we do it here.
            // but in prod, init() should already be called.
            if (UNLIKELY(!pool)) {
                init(50000); 
            }
            root = createNode(true);
        }

        ValueType find(KeyType key) {
            Node* curr = root;
            // internal node traversal (still scalar linear scan)
            while (!curr->is_leaf) {
                int i = 0;
                while (i < curr->num_keys && key >= curr->keys[i]) i++;
                curr = curr->children[i];
            }
            
            // use simd to find in leaf
            int idx = findIndexSIMD(curr, key);
            if (idx != -1) return curr->values[idx];
            return -1;
        }

        void insert(KeyType key, ValueType value) {
            Node* new_child = nullptr;
            KeyType median = 0;
            insert_recursive(root, key, value, new_child, median);
            
            // if root split create new root
            if (new_child) {
                Node* new_root = createNode(false);
                new_root->keys[0] = median;
                new_root->children[0] = root;
                new_root->children[1] = new_child;
                new_root->num_keys = 1;
                root = new_root;
            }
        }
    };
} 


/*

This is the benchmarks we got 

========================================================================
  SIMD B+ Tree Benchmark (M=256, Keys=long long)
  Operations per Scenario: 250000
========================================================================
Scenario                   P50       P90       P99       P99.9       Avg
------------------------------------------------------------------------
Sequential Write           121       138       158         625       121 ns
Sequential Read             50        65        85        1338        54 ns
Random Write               156       261       391        1257       177 ns
Random Read                108       142       570        1834       124 ns
========================================================================


*/