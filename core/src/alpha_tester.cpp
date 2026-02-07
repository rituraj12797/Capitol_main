// this is the test strategy engine.

#pragma once 

#include "lf_queue.h"
#include "lob_structs.h"
#include "order_gateway_structs.h"

namespace internal_lib {
	class AlphaServer {

		private : 
			internal_lib::LFQueue<internal_lib::UserOrder>* AlphaOrderQueue;
			internal_lib::LFQueue<internal_lib::UserAcknowledgement>* UserAcknowledgementQueue;

			internal_lib::LFQueue<internal_lib::BroadcastElement>* BroadcastQueue;
			std::vector<internal_lib::UserOrder> TestStore;

			int count_increment = 0;

		public : 

			AlphaServer(
				internal_lib::LFQueue<internal_lib::UserOrder>* aoq,
				internal_lib::LFQueue<internal_lib::UserAcknowledgement>* uaq,
				internal_lib::LFQueue<internal_lib::BroadcastElement>* bq
				) 
				:
				AlphaOrderQueue(aoq),
				UserAcknowledgementQueue(uaq),
				BroadcastQueue(bq)
				{}; // empty constructor


			void AlphaRun(std::atomic<bool>& start,std::atomic<bool>& terminate) noexcept {
				//  hogging

				// fill testStore here with 10,000 order entries each with price between 110 to 150 and price is quantum osd 0.1 so it can be anythingx*0.1 which lies betwen 110 amd 150


				for (int i = 0; i < 10000; ++i) {
    				internal_lib::UserOrder order;
    				// (150 - 110) / 0.1 = 400 steps. 
    				// rand() % 401 generates 0-400. 
    				// 110 + (result * 0.1) gives the price.
    				order.price = 110.0 + (double)(rand() % 401) * 0.1; 
    				TestStore.push_back(order);
				}

				while(!start.load(std::memory_order_acquire)){
					if(terminate.load(std::memory_order_acquire)) return;
				}

				// start
				for(int i = 0 ; i < TestStore.size() && !terminate.load(std::memory_order_acquire); i++) {
					// std::cout<<" run ";
					run(i);
				}

				std::cout<<" 10,000 Orders Sent from A -> G \n";
				std::cout<<" Increments received from Broadcaster : "<<count_increment<<"\n";
				
			}

			void run(int& i) noexcept {
				// send order to ordergateway
				// this should have been buys wait but since we knwo our queues won't content the have size in millions and tghe tests are few thousand only

				internal_lib::UserOrder* write = AlphaOrderQueue->getNextWrite();
				if(write == nullptr) {
				 	write = AlphaOrderQueue->getNextWrite(); // retry
				}

				// probably we can write now.
				*write = TestStore[i];
				// idealyly a busy wait or a spin wait should have been here but okay we can run for now
				AlphaOrderQueue->updateWrite();



				// read from incremental change log,
				auto* broadcastIncrement = BroadcastQueue->getNextRead();
				if (broadcastIncrement) {
					count_increment++;
    				BroadcastQueue->updateRead();
				}

				// no ned to process it just let it sink in
				// read from acknoweldgements
				auto* ackBack = UserAcknowledgementQueue->getNextRead();
				if (ackBack) {
    				UserAcknowledgementQueue->updateRead();
				}
				// let it sink in
			}
	};
}