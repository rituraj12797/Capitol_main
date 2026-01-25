#pragma once

#include <iostream>
#include <thread>
#include <atomic>
#include <string>
#include <chrono>


// THIS CODE IS WRITTEN WITH HELP OF AGENTIC AI - AS I ALREADY WROTE THIS PIECE OF CODE EARLIER FOR WINDOWS I DECIDED NOT TO THE MANUAL WORK AGAIN FROM SCRATCH



// ============================================================
// OS-SPECIFIC HEADERS    ==> Specifically for Linux
// ============================================================
#ifdef __linux__
    #include <pthread.h>
    #include <sched.h>
#endif

namespace internal_lib {

    /**
     * Sets the Core Affinity for the CURRENT thread.
     * * @param core_id The specific CPU core index (0, 1, 2...) to pin this thread to.
     * @return true if successful, false otherwise.
     */
    inline bool setThreadCoreAffinity(int core_id) {
#ifdef __linux__
        // 1. Create a CPU set (empty)
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);

        // 2. Add the specific core_id to the set
        CPU_SET(core_id, &cpuset);

        // 3. Get the native handle for the current thread
        pthread_t current_thread = pthread_self();

        // 4. Set affinity (returns 0 on success)
        int result = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
        return (result == 0);
#else
        // If you ever compile on Windows, you can add the #elif _WIN32 logic here
        std::cerr << "Core affinity not supported on this OS yet.\n";
        return false;
#endif
    }

    /**
     * Creates a thread, pins it to a core, and waits until it is confirmed running.
     * * @param core_id Core to pin the thread to.
     * @param name Name for logging purposes.
     * @param func The function/callable to execute.
     * @param args Arguments to pass to the function.
     * @return Pointer to the std::thread object (caller must delete it).
     */
    template<typename T, typename... A>
    inline std::thread* createAndStartThread(int core_id, const std::string &name, T &&func, A &&... args) noexcept {
        
        // These flags track the startup status of the new thread
        auto running = new std::atomic<bool>(false);
        auto failed = new std::atomic<bool>(false);

        // Define the thread body lambda
        // We capture 'args' by value [=] to ensure they are copied to the new thread's stack.
        // We use 'mutable' because we modify the captured 'func' via std::forward
        auto thread_body = [=, func = std::forward<T>(func)]() mutable {
            
            // 1. Attempt to Pin the Thread
            if (core_id >= 0 && !setThreadCoreAffinity(core_id)) {
                std::cerr << "Failed to set Core affinity for: " << name << " on Core " << core_id << "\n";
                *failed = true;
                return;
            }

            // 2. Signal success
            std::cout << "Thread [" << name << "] pinned to Core " << core_id << "\n";
            *running = true;

            // 3. Execute the actual user function
            func(args...);
        };

        // Create the thread
        std::thread* t = new std::thread(thread_body);

        // Wait for the thread to either start successfully or fail
        while (!(*running) && !(*failed)) {
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(1ms);
        }

        // Cleanup the atomic flags (we allocated them, we delete them)
        // Note: In a robust system, we would use smart pointers or a struct for context
        // but for this specific pattern, we clean up here if we detach, 
        // BUT since the lambda captures the pointers by value, we actually rely on the lambda 
        // finishing before we could safely delete them. 
        // FIX: Ideally, 'running' and 'failed' should be std::shared_ptr or passed via a specialized struct
        // to avoid memory leaks or dangling pointers. 
        // For now, to keep your logic simple and matching your snippet, we assume 
        // the main thread waiting here is sufficient synchronization.
        
        // If it failed to set affinity, we kill the thread object
        if (*failed) {
            if (t->joinable()) t->join();
            delete t;
            t = nullptr;
        }

        // Clean up our synchronization primitives
        delete running;
        delete failed;

        return t;
    }

}