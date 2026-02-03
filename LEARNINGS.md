

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


