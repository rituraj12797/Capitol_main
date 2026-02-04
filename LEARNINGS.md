

1. Static Objects instead of mempool - when we need to transferan object from one pipeline to other then to temporarly store it we can use a single object defined in stack memory only.




2. Optimized Free list - high_water_mark + free list implementation, 



3. Modulo could be replaced with bit wise and if the size could be shifted to a power of 2 making the function blazing fast.



4. It is always good to make memory layout and byte friendly structs




5. Fanout structurea are awesome and we learnt to design a Logegr 




6. We learnt how to write a LMAX level ring buffer for IPC




7. Vectorization helps !!!






8. SPSC Ring buffers are wait free, 




9. A intruisive doubly linked list whihc could be cache friendly   (does not uses pointers) -- implemented using vector, supports contant time Deletion and addition, 
wanted to use in LOB but Gliding through Dead Orders is much faster---  not me but the benchmarks say so.
rituraj12797@bellw3thers-pc:~/Capitol_main/build$ ./capitol 

--- SCENARIO: 10% DEAD ORDERS ---
Active Orders: 9000608
Gliding (Sequential Check): 20704 us  | Sum: 9000608
Jumping (Index Skipping):   36282 us  | Sum: 9000608
>> WINNER: GLIDING (1.75241x faster)

--- SCENARIO: 50% DEAD ORDERS ---
Active Orders: 5001408
Gliding (Sequential Check): 21272 us  | Sum: 5001408
Jumping (Index Skipping):   37639 us  | Sum: 5001408
>> WINNER: GLIDING (1.76942x faster)

--- SCENARIO: 90% DEAD ORDERS ---
Active Orders: 1000837
Gliding (Sequential Check): 20719 us  | Sum: 1000837
Jumping (Index Skipping):   42815 us  | Sum: 1000837
>> WINNER: GLIDING (2.06646x faster)

--- SCENARIO: 99% DEAD ORDERS ---
Active Orders: 100041
Gliding (Sequential Check): 20850 us  | Sum: 100041
Jumping (Index Skipping):   11421 us  | Sum: 100041
>> WINNER: JUMPING (1.82558x faster)


Code for this could be find in the main.cpp of this commit 


10. 

In multiplexed wtiting systems ---> always Publish to the Future (Destinations) before you Release the Past (Source).

Writing and commit before updating reading index is a good practice, since it ensures that data is actually produced


```
Why this	
							BroadcastToMarketMaker->updateWrite();
							BroadcastToAlphaEngine->updateWrite();
							BroadcastQueue->updateRead();
is better and 
not this in the broadcaster --->
							BroadcastQueue->updateRead();
							BroadcastToMarketMaker->updateWrite();
							BroadcastToAlphaEngine->updateWrite();


```


1. The Issues with "Read-First, Write-Second"
Priority Inversion: You are signaling the "Producer" (Matching Engine) to do more work before signaling the "Consumer" (Alpha Engine) to start processing. In a Sniper, every nanosecond counts; you want the Consumer to know there is data the microsecond it's available.

Bus Contention (Traffic Jams): By releasing the source slot first, you invite the Matching Engine to start writing new data immediately. This floods the CPU's memory bus with traffic. If you try to send your "Success" signal to the Alpha Engine in the middle of this flood, it can face "Micro-Jitter" or small delays at the hardware level.

Lost Backpressure: You lose the ability to throttle the Matching Engine. If the Alpha Engine is slow, the Matching Engine keeps producing until the entire queue is full, causing your strategy to "lag" behind the real-time state of the market.

2. How Atomics Help (The Checkpoint)
Atomics act as Memory Barriers (or Fences). They prevent the CPU and the Compiler from being "too clever."

Prevention of Reordering: When you update an atomic pointer, the CPU is forbidden from moving any memory writes (the data copy) from above that line to below it.

Visibility Guarantee: Atomics ensure that when Core 2 (Broadcaster) changes a value, that change is "flushed" out of the local store buffer and becomes visible to Core 5 (Alpha Engine) in a deterministic order.

3. The Benefits of "Write-First, Read-Second"
A. In Terms of Latency (The Signal Lead)
By calling updateWrite() first, you prioritize the Downstream component. You are giving the Alpha Engine the "Green Light" to start its calculations while the Broadcaster is still finishing its own bookkeeping. This shaves off those final few nanoseconds from the "Tick-to-Trade" loop.

B. In Terms of Backpressure (The Pipeline Sync)
This creates a "Lock-Step" mechanism. The Matching Engine can only take a new slot if the Broadcaster has successfully "handed off" the previous increment to both the Alpha Engine and the Market Maker.

It prevents the Matching Engine from "Running Away" and filling the buffers with stale data.

It ensures the Sniper is always working on the most recent possible data.
