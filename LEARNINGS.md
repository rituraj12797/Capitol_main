

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




11. A way to make your server/engines fast is to---> implement a turbo wait(kind of )---> Hold the breaks but keep the acclerator at full---> hold the execution in a busy/spining while loop and release it as soon as start flag turns true --> to start from max CPU freq.





12. Study more about memory ordering/ memory fencing in c++ and in general OS.

In our drift race/ start pattern for components we use memory_order_release on producer(main.cpp) to ensure that all the instruction before storing a value into a atomic var is complete and will be visible ( else reordering would have done somehthing out of order).

and on the consumer( Order Gateway/ matching engine ) memory_order_acquire is guaranteed that once it sees the start_signal as true, it will also see all the prep work the Producer did.

so they will see the atomic variable start_... as set only when it is set truly(and not when due to instruction re ordering).

BUT THIS HAPPENS BY DEFAULT THEN WHY WE CHOOSE TO DO IT ??
by default atomic uses memory_order_global ---> which enforces this security across all cores to maintain atomic works in order----> but this is costly

so we do this only but kind of at local level between producer and consumer thread.






13. Throttling- to make the Alpha Slow during benchmarking 
14. ingress throttling - to make the Order gateway add some processing latency so that it becomes the biottle nech and hence the LKFqueue inside the system now won;t be choked only sniper queue will be choked and system will be like a free glide highway .



15. Adaptive Backpressure is a thing - warpstream is somnething related to it ?? 



16. for microbenchmarkign at extreme accuracy we use __rdtsc() read_time_stamp_counter ---> a way to get the number of CPU cycles completed without system call --->  usually chrono::now() is slow due to sys call involvement.

but this __rdtsc() is non serializable --> means due to OOO execution ( out of order execution )  you may get wrong timings to add serializability to it we use ---> lfence _ rdtscp() + lfence ----> 
