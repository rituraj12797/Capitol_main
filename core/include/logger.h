#pragma once 

#include<fstream>
#include<thread>
#include<atomic>
#include <variant>
#include <string>

#include "lf_queue.h"
#include "time_util.h"
#include "imp_macros.h"
#include "lob_structs.h"
#include "order_gateway_structs.h"


namespace internal_lib {

	// define a Log structure 

	//	   We Mainly Log 5 type Of Element 

	struct LogElement {

		int log_identifier; // identifier of event
		uint64_t time_stamp; // when did this event happened 
		// container 
		std::variant<internal_lib::BroadcastElement, internal_lib::LOBOrder, internal_lib::UserOrder, internal_lib::LOBAcknowledgement, internal_lib::UserAcknowledgement> logData; // data associated with this event 
	};

	// 1. LOBOrder
	// 2. UserOrder
	// 3. LOBAcknowledgement
	// 4. UserAcknowldegement
	// 5. BroadCastElement 


	// What Are The Events That We Will Log ??

	// Order Arrives At Order Gateway ==> Identifier = 1 ===> Log Data = User Order 
	// Order Is Sent From Order GateWay To Matching Engine =====> Identifier = 2 ====> Log Data = LOBOrder
	// Order Arrived At Matching Engine ====> Identifier = 3 ====> Log Data = LOBOrder
	// Order Aggressive Matched ====> Identifier = 4 ====> Log Data = LOBOrder
	// Order Created In LOB ====> Identifier = 5 ====> Log Data = LOBOrder
	// Order Deleted In LOB ====> Identifier = 6 ====> Log Data = LOBOrder
	// Order Quantity Update in LOB ====> Identifier = 7 ====> Log Data = LOBOrder
	// WHY NO LOGGING FOR PRICE CHANGE BASED UPDATES ?? ====? beacuse it involved creation and then deletion which will log this event so no need to do again.
	// Matching Engine Passed An Acknowledgement To Order Gateway====> Identifier = 8 ======> Log Data = LOBAcknowledgement
	// Order Gateway Received An Acknowledgement ====> Identifier = 9 =====> Log Data = LOBAcknowledgement
	// Order Gateway Sends An Acknowledgement To Sniper/Alpha ====> Identifier = 10 =====> Log Data = UserAcknowledgement
	// Matching Engine Broadcasts A Incremental Change To Alpha/Sniper ====> Identifier = 11 ====> Log Data = BroadcastElement

    // done - 10, 9, 8, 1, 2, 3, 4, 5, 6, 7


	// this captures all the activity happening inside our element 



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

		internal_lib::LFQueue<LogElement>* matching_engine_queue; // pointer to limited order book  logger
		internal_lib::LFQueue<LogElement>* order_gateway_queue; // pointer to network gate way logger

		std::string file_path;
		std::atomic<bool> running = {true};


		public : 


		Async_Logger(std::string& path,
					internal_lib::LFQueue<LogElement>* lbq,
					internal_lib::LFQueue<LogElement>* ntgwq): matching_engine_queue(lbq), order_gateway_queue(ntgwq), file_path(path) {
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

		 		busy |= drainBatch(matching_engine_queue, file, 50 );
		 		busy |= drainBatch(order_gateway_queue, file, 50 );

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

		char* write_string(const char* str, char* buffer) {
			while (*str) {
				*buffer++ = *str++;
			}
			return buffer;
		}

		char* fast_int_to_str(int value, char* buffer) {
			if (value < 0) {
				*buffer++ = '-';
				value = -value;
			}
			
			char temp[12];
			char* p = temp + 11;
			*p = '\0';
			
			do {
				*--p = (value % 10) + '0';
				value /= 10;
			} while (value > 0);
			
			while (*p) *buffer++ = *p++;
			return buffer;
		}

		char* fast_float_to_str(float value, char* buffer) {
			// Simple float to string conversion (2 decimal places)
			int int_part = (int)value;
			buffer = fast_int_to_str(int_part, buffer);
			*buffer++ = '.';
			
			int frac_part = (int)((value - int_part) * 100);
			if (frac_part < 0) frac_part = -frac_part;
			
			*buffer++ = (frac_part / 10) + '0';
			*buffer++ = (frac_part % 10) + '0';
			
			return buffer;
		}

		// Helper functions to write each struct type
		char* write_LOBOrder(const LOBOrder& order, char* buffer) {
			buffer = write_string("LOBOrder arrived_cycle_count : ", buffer);
			buffer = fast_u64_to_str(order.arrived_cycle_count, buffer);
			*buffer++ = '\n';
			
			buffer = write_string("LOBOrder system_id : ", buffer);
			buffer = fast_int_to_str(order.system_id, buffer);
			*buffer++ = '\n';
			
			buffer = write_string("LOBOrder price : ", buffer);
			buffer = fast_float_to_str(order.price, buffer);
			*buffer++ = '\n';
			
			buffer = write_string("LOBOrder quantity : ", buffer);
			buffer = fast_int_to_str(order.quantity, buffer);
			*buffer++ = '\n';
			
			buffer = write_string("LOBOrder trader_id : ", buffer);
			buffer = fast_int_to_str(order.trader_id, buffer);
			*buffer++ = '\n';
			
			buffer = write_string("LOBOrder order_type : ", buffer);
			*buffer++ = order.order_type;
			*buffer++ = '\n';
			
			buffer = write_string("LOBOrder req_type : ", buffer);
			*buffer++ = order.req_type;
			*buffer++ = '\n';
			
			buffer = write_string("LOBOrder out_cycle_count : ", buffer);
			buffer = fast_u64_to_str(order.out_cycle_count, buffer);
			*buffer++ = '\n';
			
			return buffer;
		}

		char* write_UserOrder(const UserOrder& order, char* buffer) {
			buffer = write_string("UserOrder arrived_cycle_count : ", buffer);
			buffer = fast_u64_to_str(order.arrived_cycle_count, buffer);
			*buffer++ = '\n';
			
			buffer = write_string("UserOrder order_id : ", buffer);
			buffer = fast_int_to_str(order.order_id, buffer);
			*buffer++ = '\n';
			
			buffer = write_string("UserOrder trader_id : ", buffer);
			buffer = fast_int_to_str(order.trader_id, buffer);
			*buffer++ = '\n';
			
			buffer = write_string("UserOrder order_type : ", buffer);
			*buffer++ = order.order_type;
			*buffer++ = '\n';
			
			buffer = write_string("UserOrder req_type : ", buffer);
			*buffer++ = order.req_type;
			*buffer++ = '\n';
			
			buffer = write_string("UserOrder price : ", buffer);
			buffer = fast_float_to_str(order.price, buffer);
			*buffer++ = '\n';
			
			buffer = write_string("UserOrder quantity : ", buffer);
			buffer = fast_int_to_str(order.quantity, buffer);
			*buffer++ = '\n';
			
			buffer = write_string("UserOrder out_cycle_count : ", buffer);
			buffer = fast_u64_to_str(order.out_cycle_count, buffer);
			*buffer++ = '\n';
			
			return buffer;
		}

		char* write_LOBAcknowledgement(const LOBAcknowledgement& ack, char* buffer) {
			buffer = write_string("LOBAcknowledgement system_id : ", buffer);
			buffer = fast_int_to_str(ack.system_id, buffer);
			*buffer++ = '\n';
			
			buffer = write_string("LOBAcknowledgement price : ", buffer);
			buffer = fast_float_to_str(ack.price, buffer);
			*buffer++ = '\n';
			
			buffer = write_string("LOBAcknowledgement quantity : ", buffer);
			buffer = fast_int_to_str(ack.quantity, buffer);
			*buffer++ = '\n';
			
			buffer = write_string("LOBAcknowledgement side : ", buffer);
			*buffer++ = ack.side;
			*buffer++ = '\n';
			
			buffer = write_string("LOBAcknowledgement status : ", buffer);
			*buffer++ = ack.status;
			*buffer++ = '\n';
			
			return buffer;
		}

		char* write_UserAcknowledgement(const UserAcknowledgement& ack, char* buffer) {
			buffer = write_string("UserAcknowledgement order_id : ", buffer);
			buffer = fast_u64_to_str(ack.order_id, buffer);
			*buffer++ = '\n';
			
			buffer = write_string("UserAcknowledgement price : ", buffer);
			buffer = fast_float_to_str(ack.price, buffer);
			*buffer++ = '\n';
			
			buffer = write_string("UserAcknowledgement quantity : ", buffer);
			buffer = fast_int_to_str(ack.quantity, buffer);
			*buffer++ = '\n';
			
			buffer = write_string("UserAcknowledgement side : ", buffer);
			*buffer++ = ack.side;
			*buffer++ = '\n';
			
			buffer = write_string("UserAcknowledgement status : ", buffer);
			*buffer++ = ack.status;
			*buffer++ = '\n';
			
			return buffer;
		}

		char* write_BroadcastElement(const BroadcastElement& broadcast, char* buffer) {
			buffer = write_string("BroadcastElement system_id : ", buffer);
			buffer = fast_int_to_str(broadcast.system_id, buffer);
			*buffer++ = '\n';
			
			buffer = write_string("BroadcastElement price : ", buffer);
			buffer = fast_float_to_str(broadcast.price, buffer);
			*buffer++ = '\n';
			
			buffer = write_string("BroadcastElement quantity : ", buffer);
			buffer = fast_int_to_str(broadcast.quantity, buffer);
			*buffer++ = '\n';
			
			buffer = write_string("BroadcastElement side : ", buffer);
			*buffer++ = broadcast.side;
			*buffer++ = '\n';
			
			buffer = write_string("BroadcastElement type : ", buffer);
			*buffer++ = broadcast.type;
			*buffer++ = '\n';
			
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

        		*offset++ = '\n'; // start from a new line


        		offset = fast_u64_to_str(elem->time_stamp, offset); // write the number into the buffer by pointer movement and allocating
        		*offset++ = ' '; // add a space to the where the write position was pointing to and then incrment the write pointer 
        		offset = fast_u64_to_str(elem->log_identifier, offset); // add message id 
        		*offset++ = ' '; // add another space and move on.
        		*offset++ = '\n'; // start from a new line

        		// handling the data object  here 

        		switch (elem->log_identifier) {

					case 1:
						// Order Arrives At Order Gateway ==> Identifier = 1 ===> Log Data = User Order 
						offset = write_UserOrder(std::get<UserOrder>(elem->logData), offset);
                 		break;

					case 2:
						// Order Is Sent From Order GateWay To Matching Engine =====> Identifier = 2 ====> Log Data = LOBOrder
						offset = write_LOBOrder(std::get<LOBOrder>(elem->logData), offset);
                 		break;

					case 3:
						// Order Arrived At Matching Engine ====> Identifier = 3 ====> Log Data = LOBOrder
						offset = write_LOBOrder(std::get<LOBOrder>(elem->logData), offset);
                 		break;

					case 4:
						// Order Aggressive Matched ====> Identifier = 4 ====> Log Data = LOBOrder
						offset = write_LOBOrder(std::get<LOBOrder>(elem->logData), offset);
                 		break;

					case 5:
						// Order Created In LOB ====> Identifier = 5 ====> Log Data = LOBOrder
						offset = write_LOBOrder(std::get<LOBOrder>(elem->logData), offset);
                 		break;

					case 6:
						// Order Deleted In LOB ====> Identifier = 6 ====> Log Data = LOBOrder
						offset = write_LOBOrder(std::get<LOBOrder>(elem->logData), offset);
                 		break;

					case 7:
						// Order Quantity Update in LOB ====> Identifier = 7 ====> Log Data = LOBOrder
						offset = write_LOBOrder(std::get<LOBOrder>(elem->logData), offset);
                 		break;

					case 8:
						// Matching Engine Passed An Acknowledgement To Order Gateway====> Identifier = 8 ======> Log Data = LOBAcknowledgement
						offset = write_LOBAcknowledgement(std::get<LOBAcknowledgement>(elem->logData), offset);
                 		break;

					case 9:
						// Order Gateway Received An Acknowledgement ====> Identifier = 9 =====> Log Data = LOBAcknowledgement
						offset = write_LOBAcknowledgement(std::get<LOBAcknowledgement>(elem->logData), offset);
                 		break;

					case 10:
						// Order Gateway Sends An Acknowledgement To Sniper/Alpha ====> Identifier = 10 =====> Log Data = UserAcknowledgement
						offset = write_UserAcknowledgement(std::get<UserAcknowledgement>(elem->logData), offset);
                 		break;

					case 11:
						// Matching Engine Broadcasts A Incremental Change To Alpha/Sniper ====> Identifier = 11 ====> Log Data = BroadcastElement
						offset = write_BroadcastElement(std::get<BroadcastElement>(elem->logData), offset);
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



