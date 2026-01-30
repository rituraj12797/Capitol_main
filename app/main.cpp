#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <sched.h>
#include <pthread.h>
#include <chrono>

#include "lf_queue.h"
#include "logger.h"
#include "market_data_publisher.h"
#include "order_gateway.h"
#include "matching_engine.h"
#include "market_participant.h"
#include "simd_b_plus_tree.h"

// --- GLOBAL ARENA ---
Arena* global_arena = nullptr;

// --- CONFIGURATION ---
const int QUEUE_SIZE = 1024 * 1024; // 1 Million Capacity buffers
const int BENCHMARK_ORDERS = 1000000;

// --- CORE PINNING HELPER ---
void pin_thread_to_core(int core_id) {
    if (core_id < 0) return; // Disable pinning if -1

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t current_thread = pthread_self();
    int rc = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
    
    if (rc != 0) {
        std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
    } else {
        // std::cout << "Thread pinned to Core " << core_id << "\n";
    }
}

int main() {
    std::cout << "=== CAPITOL HFT ENGINE INITIALIZING ===\n";

    // 1. Initialize Memory Arena (1GB)
    global_arena = new Arena(1024 * 1024 * 1024);

    // 2. Initialize Queues (The Ring Buffers)
    // User <-> Gateway
    internal_lib::LFQueue<ClientRequest> q_user_to_gw(QUEUE_SIZE);
    internal_lib::LFQueue<ServerResponse> q_gw_to_user(QUEUE_SIZE);

    // Gateway <-> Engine
    internal_lib::LFQueue<LOBRequest> q_gw_to_lob(QUEUE_SIZE);
    internal_lib::LFQueue<LOBResponse> q_lob_to_gw(QUEUE_SIZE);

    // Engine <-> MktPublisher <-> User
    internal_lib::LFQueue<Trade> q_lob_to_pub(QUEUE_SIZE);
    internal_lib::LFQueue<MarketUpdate> q_pub_to_user(QUEUE_SIZE);

    // 3. Instantiate Components
    // Core 3: Gateway
    // HEAP ALLOCATED to avoid Stack Overflow (16MB internal array)
    auto gateway_ptr = std::make_unique<OrderGateway>(q_user_to_gw, q_gw_to_user, q_gw_to_lob, q_lob_to_gw);
    OrderGateway& gateway = *gateway_ptr;
    
    // Core 2: Engine
    // HEAP ALLOCATED to avoid Stack Overflow (88MB internal arrays)
    auto engine_ptr = std::make_unique<MatchingEngine>(q_gw_to_lob, q_lob_to_gw, q_lob_to_pub);
    MatchingEngine& engine = *engine_ptr;
    
    // Core 1: Publisher
    MarketDataPublisher publisher(q_lob_to_pub, q_pub_to_user);
    
    // Core 4: User (Participant)
    MarketParticipant user(q_user_to_gw, q_gw_to_user, q_pub_to_user, 1);

    // 4. Launch Threads
    std::vector<std::thread> thread_pool;

    // PUBLISHER
    thread_pool.emplace_back([&]() {
        pin_thread_to_core(1);
        publisher.run();
    });

    // ENGINE
    thread_pool.emplace_back([&]() {
        pin_thread_to_core(2);
        engine.run();
    });

    // GATEWAY
    thread_pool.emplace_back([&]() {
        pin_thread_to_core(3);
        gateway.run();
    });

    // USER (BENCHMARK)
    thread_pool.emplace_back([&]() {
        pin_thread_to_core(4);
        
        // Start helper thread to drain responses quietly so queue doesn't fill
        // Actually user.run() does this, but we want to measure send speed.
        // Let's launch user receiving in background or just let user run do both.
        // For accurate OPS, we need separate logic here or use the class.
        
        std::cout << "[BENCHMARK] Warming up...\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::cout << "[BENCHMARK] Sending " << BENCHMARK_ORDERS << " orders...\n";
        
        auto start = std::chrono::high_resolution_clock::now();

        // Burst Send
        double price = 100.00;
        for(int i=0; i < BENCHMARK_ORDERS; i++) {
             // Oscillate price to create matches?
             // Or just spam resting orders to test Insertion Speed
             // Let's spam resting orders first (Book Building)
             user.sendBuyOrder(price, 10);
             price += 0.01;
             
             if (i % 1000 == 0) {
                 // Briefly poll to drain ACKs so we don't block
                 // In a real test, a separate thread would drain.
                 // We will rely on large queue (1M) to absorb burst.
             }
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;

        std::cout << "[BENCHMARK] Send Completed.\n";
        std::cout << "Time: " << diff.count() << " s\n";
        std::cout << "Rate: " << (BENCHMARK_ORDERS / diff.count()) << " Orders/Sec\n";

        // Keep alive to drain
        // user.run(); 
        
        // Since we didn't call user.run(), we need to manually drain or stop
        std::this_thread::sleep_for(std::chrono::seconds(2));
        exit(0);
    });

    // Join (Will block forever)
    for(auto& t : thread_pool) {
        if(t.joinable()) t.join();
    }

    return 0;
}
