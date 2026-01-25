#pragma once 
#include<fstream>
#include<thread>
#include<atomic>

#include "lf_queue.h"
#include "time_util.h"
#include "imp_macros.h"


namespace internal_lib {

	enum class ComponentId : uint8_t { // this defines a enum of size 1 Byte, which denotes the log is sent by which process
		MKT_DATA = 0, // if ComponentId becomes 1 means the log was sent by the mkt data publisher.
		LOB_ENGINE = 1,	// if 1 then set by the Limited Order Book Matching Engine
		ORDER_GATEWAY = 2,	// if 2 then sent by the order gateway
		SYSTEM_CORE = 3 // if 3 means it is a custom system log
	};


	// market data
	struct market_data_publisher_log_object { // this will be the data object that the market publishers logs
		// WE WILL DEFINE THIS WHEN WE WRITE THE MARKET DATA PUBLISHER COMPONENT 
		// for now only keep a sinhle int 
		int x;
	};


	struct limited_order_book_log_object { // this will be the data object lob logs
		// WE WILL DEFINE THIS WHEN WE WRITE THE LOB COMPONENT  
		int y;
	};

	struct network_order_gateway_log_object { // will be the log data whihc network gateway object logs 
		// WE WILL DEFINE THIS WHEN WE WRITE THE ORDER GATEWAY COMPONENT 
		int z;
	};

	// we make our LogElement 64 bytes long in order to make it fit exactly in one cache line- so that when data pull.push happens complete data gets picked up
	struct alignas(64) LogElement {

		uint64_t time_stamp; // timestamp  // 8 bytes 
		ComponentId component; // 1 byte
		int32_t core_id; // 4 bytes 
		int32_t string_token; // 4 bytes for string token; 

		// the log data container
		union {
			market_data_publisher_log_object mkt;
			limited_order_book_log_object lob;
			network_order_gateway_log_object ogw;
			int64_t generic_data; // for storing custom integer data;
		} data_object;
	};


	// define a logger class 

	// main optimizations :- 
	// 1. the 3 producer one consumer architecture of logger - Fan In - the consumer rotates in the round robin manner around th 3 producers
	// 2. batching -> we dintjus insert each entry from queue to the log file one by one, instead we collect them and when a certain threshold of them is in our hands 
	// we persist them via a single system call

	// 3. In batching to store these elements we add them to a string but dynamically adding to string uses heap allocation whihc might be slow so we define 
	// a charecter buffer of say 4KB which lives in stack memory hence it is fast and accumulate teh log entries there
	// once the batch is processed we flush this stack memmory into the log file



	// what is the architecture now ?? 
	// well it's a bit complex but we will try to understand it 
	// The main only will have all the queues in the system andno other thread will make any queue 
	// so these 3 LOG_QUEUES will also be defined into the main function 
	// the 3 performance critical threads each will take one queue with reference
	// the 4th logger thread will get all the 3 queues as input as refercne 

	// so the owner is main --> it provided the reference to the writers to write and the readers to read from this way queues remain as logn as server runs 
	// and no allocation issues arise 


	// this is dependency injection ===> an object receives it's dependency instead of creating it for itself. ( Design pattern )

	class Async_Logger  {

		private : 

		internal_lib::LFQueue<LogElement>* mk_pub_queue; // pointer to the market publisher service logger
		internal_lib::LFQueue<LogElement>* lob_queue; // pointer to limited order book  logger
		internal_lib::LFQueue<LogElement>* network_gw_queue; // pointer to network gate way logger

		std::string file_path;
		std::atomic<bool> running = {true};


		public : 


		Async_Logger(std::string& path,
					internal_lib::LFQueue<LogElement>* mkpbq,
					internal_lib::LFQueue<LogElement>* lbq,
					internal_lib::LFQueue<LogElement>* ntgwq) : mk_pub_queue(mkpbq), lob_queue(lbq), network_gw_queue(ntgwq), file_path(path) {
			// empty body here 	
		}

		void stop() noexcept {
			running = false;
		}

		void run() noexcept {
			std::ofstream file(file_path, std::ios::out | std::ios::trunc); // if the file exists it clears the content inside it if it doesnt exists it will create a new one 
		 	
		 	ASSERT(file.is_open(), " FILE not accessible ");

		 	std::vector<char> buff(128*1024); // defined a custom buffer of size 128 kb;

		 	file.rdbuf()->pubsetbuf(buff.data(),buff.size());
		 	// MACRO OPTIMIZATION 

		 	// we do a tweaking here 
		 	// normally when we do ofstream << or file << it does nto write to disk at that moment it stores that data into it's buffer
		 	// now it's buffer is by default of size 4kb or 8kb which gets fulled very quickly

		 	// so means more sys calls to flush this to  disk due to shorter size
		 	// so we make the size of it to large to that it can flush large chunks of data in one system call

		 	while(running) {

		 		bool busy  = false; // define a buys variable 

		 		busy |= drainBatch(mk_pub_queue, file, 50 );
		 		busy |= drainBatch(lob_queue, file, 50 );
		 		busy |= drainBatch(network_gw_queue, file, 50 );

		 		if(busy == false) std::this_thread::yield();
		 	}

		 	// in the while loop when say the 128kb buffer will fill in one write what will happen is system call will be made you process wil be stopepd 
		 	// the buffer will flush this data to the disk 
		 	// process continues and the new writes data will be filled in this buffer again. this we need not to handle it is automatically taken care of 


		 	file.flush(); // when we are done running and the server is closing now we would like to collect any remaining data in the buffer and store it into the disk so we flush it one last time
		}

		// on a given offset write the number digit by digit on write pointer's memory nd keeps incrementing the ppointer
		char* fast_u64_to_str(uint64_t value, char* buffer) {

    		char temp[24];
    		char* p = temp + 23;
    		*p = '\0';

    		do {
        		*--p = (value % 10) + '0';
        		value /= 10;
    		} while (value > 0);
    
    		while (*p) *buffer++ = *p++;
    		return buffer;
		}

		bool drainBatch(LFQueue<LogElement>* q, std::ofstream& file, int limit) {

			// define a 4kb stack buffer
    		char buffer[4096]; 
    		char* offset = buffer; // current write position
    		char* end = buffer + sizeof(buffer) - 128; // safe margin

    		int count = 0;
    		
    		// untill the batch processing completes or buffer is full 
    		while (count < limit && offset < end) {
        		LogElement* elem = q->getNextRead(); // read next element
        		if (!elem) break; // nul waiting so quit

        		offset = fast_u64_to_str(elem->time_stamp, offset); // write the number into the buffer by pointer movement and allocating
        		*offset++ = ' '; // add a space to the where the write position was pointing to and then incrment the write pointer 
        		offset = fast_u64_to_str(elem->string_token, offset); // add message id 
        		*offset++ = ' '; // add another space and move on.

        		// handling the data object  here 
        		switch (elem->component) {
        			// handling market published data log object
            		case ComponentId::MKT_DATA:
                		 offset = fast_u64_to_str(elem->data_object.mkt.x, offset); // add the data object -> markety_data_publisher_log_object.x to it 
                 		*offset++ = ' ';
                 		break;
                 	// handling generic log object
            		case ComponentId::SYSTEM_CORE:
                	 	offset = fast_u64_to_str(elem->data_object.generic_data, offset);
                	 	*offset++ = ' ';
                 		break;
                 	// handling LOB matchign log object
                 	case ComponentId::LOB_ENGINE:
                 		offset = fast_u64_to_str(elem->data_object.lob.y,offset);
                 		*offset++ = ' ';
                 		break;
                 	// handling order gateway log object
                 	case ComponentId::ORDER_GATEWAY:
                 		offset = fast_u64_to_str(elem->data_object.ogw.z,offset);
                 		*offset++ = ' ';
                 		break;
        		}
        
       		 	*offset++ = '\n'; // add a new line charecter and increment the write position and 

	       	 	q->updateRead(); // update read index in the queue 
    	   	 	count++; // one log read
    		}

    		// at last there coulkd be 2 scenarios either the offset reached end, or before the buffer was full the batch was done or the queue became empty
    		// in any case we have fetched some logs and written them untill the offset pointer in our charectr buffer 
    		// flush that into file's 128KB buffer

	    	if (offset > buffer) { 
    	    	file.write(buffer, offset - buffer);
    		}

	    	return count > 0; // if read happened then return true;
		}
	};
}



