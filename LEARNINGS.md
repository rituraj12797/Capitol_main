

1. Static Objects instead of mempool - when we need to transferan object from one pipeline to other then to temporarly store it we can use a single object defined in stack memory only.




2. Optimized Free list - high_water_mark + free list implementation, 



3. Modulo could be replaced with bit wise and if the size could be shifted to a power of 2 making the function blazing fast.



4. It is always good to make memory layout and byte friendly structs




5. Fanout structurea are awesome and we learnt to design a Logegr 




6. We learnt how to write a LMAX level ring buffer for IPC




7. Vectorization helps !!!






8. SPSC Ring buffers are wait free, 




9. A intruisive doubly linked list whihc could be cache friendly   (does not uses pointers) -- implemented using vector, supports contant time Deletion and addition, Used in LOB to do fast insertions and deletion of Orders.




