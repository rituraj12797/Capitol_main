#pragma once

#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>
#include <cstdint>

#ifdef __linux__
    #include <pthread.h>
    #include <sched.h>
#endif

namespace internal_lib {

    /**
     * hard CPU pre-warmer
     * 
     * Spawns a busy-spin thread on EVERY available core for a configurable duration.
     * this forces the CPU governor to ramp all cores to max turbo frequency 
     * before any latency-sensitive work begins.
     * 
     * the spin loop uses volatile + simple arithmetic to prevent the compiler 
     * from optimizing it away, while keeping the workload lightweight (no cache pollution).
     */

    inline void prewarm(int duration_seconds = 100) {

        unsigned int num_cores = std::thread::hardware_concurrency();
        if (num_cores == 0) num_cores = 4; // fallback

        std::cout << "[PREWARMER] burning " << num_cores << " cores for " 
                  << duration_seconds << " seconds to reach max CPU frequency\n";

        std::atomic<bool> stop_warming{false};
        std::vector<std::thread> warmers;
        warmers.reserve(num_cores);

        for (unsigned int core = 0; core < num_cores; ++core) {
            warmers.emplace_back([core, &stop_warming]() {
                // pin this warmer thread to its core
#ifdef __linux__
                cpu_set_t cpuset;
                CPU_ZERO(&cpuset);
                CPU_SET(core, &cpuset);
                pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
                // busy spin — volatile prevents the compiler from killing the loop
                volatile uint64_t sink = 0;
                while (!stop_warming.load(std::memory_order_relaxed)) {
                    sink += 1; // trivial alu work, no memory traffic
                }
                (void)sink;
            });
        }

        // let them burn
        std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));

        stop_warming.store(true, std::memory_order_relaxed);

        for (auto& t : warmers) {
            t.join();
        }

        std::cout << "[PREWARMER] done. all cores at max frequency.\n";
    }

} // namespace internal_lib
