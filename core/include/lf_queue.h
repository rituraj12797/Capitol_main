#pragma once 

// why this '#pragma once' was needed ==> so that even if in multiple files when we inlcude thi sheader the c++ should nto import multiple compies of it to give an error of redeclarations
#include<iostream>
#include<vector>
#include<atomic>
#include<thread>
// #include "internal_lib.h"
#include "imp_macros.h"



namespace internal_lib {
	template<typename T>


	class LFQueue final { // why final here ===> final is a keyword that emans no other class is going to inheit this class hence the compile devirtualizes it ==> performance gains 
	private :

		// std::atomic<size_t> next_index_to_read = {0};   
		// std::atomic<size_t> next_index_to_write = {0};

		alignas(64) std::atomic<size_t> next_index_to_read = {0};   
		alignas(64) size_t lazy_write = {0};

		std::vector<T> store_; 

		alignas(64) std::atomic<size_t> next_index_to_write = {0};
		alignas(64) size_t lazy_read = {0};
		

	public : 
		size_t buffer_size = 2;
		size_t capacity_mask = buffer_size - 1; // the mask we will use to wrap

		explicit LFQueue(std::size_t capacity ){  

			size_t target_size = capacity;
    		if(target_size < 5 * capacity) target_size = 5 * capacity; //  multiplier logic

    		buffer_size = 2;

    		while(buffer_size < target_size) {
        		buffer_size *= 2;
    		}

    		capacity_mask = buffer_size - 1;

    		store_.resize(buffer_size);
		}

		// delete extra constructors

		LFQueue() = delete;
		LFQueue(const LFQueue&) = delete;
		LFQueue(const LFQueue&&) = delete;

		LFQueue& operator = (const LFQueue&) = delete;
		LFQueue& operator = (const LFQueue&&) = delete;
		T* getNextWrite() noexcept {
			// we see is the queue is fulll then reruner thenull ptr 
			// an Important lesson here 

			if(((next_index_to_write + 1)&(capacity_mask)) == lazy_read) { // we arrived where last time read was found 
				// now found where exactlky is this read 
				if(((next_index_to_write + 1)&(capacity_mask)) == next_index_to_read) {
					return nullptr;
				}
				lazy_read = next_index_to_read;
			}

			return &(store_[next_index_to_write]);
		}

		auto updateWrite() noexcept { // no contention here as our queue is SPSC ==> sngle producer single consumer ==> only one writer to only it will uipdate the write index 

			if( ((next_index_to_write + 1)&( capacity_mask)) == lazy_read) {
				lazy_read = next_index_to_read;
				// internal_lib::ASSERT( ((next_index_to_write + 1)&( capacity_mask)) != lazy_read ," Queue full thread must wait before further writing");
			}

			next_index_to_write = ((next_index_to_write + 1)&( capacity_mask));
		}

		T* getNextRead() noexcept {
			// if the consumer consumed all the values and is now pointing to the next write index ==> means the place to which it is pointintg has no data yet so we return nullptr

			if(next_index_to_read == lazy_write) {
				if(next_index_to_read == next_index_to_write) {
					return nullptr;
				}
				lazy_write = next_index_to_write;
			}

			return &(store_[next_index_to_read]);
		}

		auto updateRead() noexcept {

			if(next_index_to_read == lazy_write) {
				lazy_write = next_index_to_write;
				// internal_lib::ASSERT(next_index_to_read != lazy_write," Nothing to consume by thread ");
			}

			next_index_to_read = ((next_index_to_read + 1)&( capacity_mask));
		}	


	};
};



// some doubts whihc i faced when building this 

// why are we defining the queue using a vector of fixed sizew ?? ==> we do so because dynamically allocating new memory at run time is low and so we define a fixed size one.
// what if it overflows ?? ===> we always have an estimate of how many requests per second are gonna come ==> say if avg is 10,000 we define the size to be 100x more than that so,
// practically it never overflows 

// also since this ia a ring buffer ==> write and read indices re start from 0 when they reach the end this will work 

// what if the reader has not read anythign and the queue is complete and now write pointer to teh head where the first piece of data is stored ===> now if you try to write to thix location you may los on the data 
// that was supposed to be read by the reader ==> CORRUPTION ?? ==> Umm not really, because for those scenarios we developed an approach ==> if any such scenarios arrive that writer head is pointing to reading head we will block the writer 
// from writing further entries untill the reader consumes data . ======> This is know as BACKPRESSURE






// there is a isssues with the write_head == read_head case  ==> this scenario is possible in 2 scenarios 
/*


	1. When circular buffer is empty => 

	             R
	_________________________
	|____|____|____|____|____|

				W----------->
	

	2. When it is full

	             R
	_________________________
	|__2_|_3__|_4__|_5__|__7_|

	----------->W

	
	in the first scenario wew would prevent Read as no data is there 
	and in the second scenatio we would prevemt the Write as we need to wait for the consumer to consume the data 

	how do we differentiate whic case it it ?? ==> for this we maintain a empty block, i.e we never fill the ring buffer complete, we always leave it one block empty i.e in full state it willlook somethign like this 

3. Leave one block - desirable full state

	             R
	_________________________
	|__2_|____|_4__|_5__|__7_|

	------>W

	now we say that untill the reader does not moves this last block will nto be filled 

	==>/    if((write + 1) %size == Read )  this will mean full 
	===>     write == read means it is empty 

*/


