#pragma once 

#include "lf_queue.h"
#include "simd_bplus_tree.h"
#include "order_gateway_structs.h"
#include "mempool.h" 

#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

namespace internal_lib {

    class OrderGateway {

		// we will use dependency injection here ====> the LF queues this order gateway is going to use will be defined in main thread only
		// at startup and will be used here via a reference 
        private : 
            // LOB Communication
            internal_lib::LFQueue<internal_lib::LOBOrder>* LobOrderQueue; 
            internal_lib::LFQueue<internal_lib::LOBAcknowledgement>* LobAckQueue; 

            // sniper communication
            internal_lib::LFQueue<internal_lib::UserOrder>* SniperOrderQueue; 
            internal_lib::LFQueue<internal_lib::UserAcknowledgement>* SniperAckQueue; 

            // Market Maker communication
            internal_lib::LFQueue<internal_lib::UserOrder>* MMOrderQueue; 
            internal_lib::LFQueue<internal_lib::UserAcknowledgement>* MMAckQueue;

            internal_lib::SIMDBPlusTree<long long, int, 256> BPTree; 
            std::vector<long long> LUT; 
            
            int next_system_id = 0; // start from 0

        public :

            OrderGateway() {
                // initialize B+ Tree
                internal_lib::SIMDBPlusTree<long long, int, 256>::init(100000);
                
                // Pre-allocate LUT
                LUT.resize(1000000);
            }

            long long SystemToOrderId(int sysId) noexcept {
                return LUT[sysId];
            }

            // logic for getting system id
            int GetOrAssignSystemId(long long orderId, char reqType) noexcept {
                if (reqType == 'c') {
                    // create new
                    int sysId = next_system_id++;
                    BPTree.insert(orderId, sysId);
                    LUT[sysId] = orderId;
                    return sysId;
                } else {
                    // lookup existing
                    return BPTree.find(orderId);
                }
            }

            void run(LFQueue<internal_lib::LOBAcknowledgement>* LobAckQueue,  
                     LFQueue<internal_lib::UserOrder>* SniperOrderQueue, 
                     LFQueue<internal_lib::UserAcknowledgement>* SniperAckQueue, 
                     LFQueue<internal_lib::UserOrder>* MMOrderQueue, 
                     LFQueue<internal_lib::UserAcknowledgement>* MMAckQueue,
                     LFQueue<internal_lib::LOBOrder>* LobOrderQueue) noexcept { 

                // update local pointers
                this->LobAckQueue = LobAckQueue;
                this->SniperOrderQueue = SniperOrderQueue;
                this->SniperAckQueue = SniperAckQueue;
                this->MMOrderQueue = MMOrderQueue;
                this->MMAckQueue = MMAckQueue;
                this->LobOrderQueue = LobOrderQueue;

                while(true) {
                    
					// take input from sniper
                    UserOrder* readOrder = SniperOrderQueue->getNextRead(); 

                    if(LIKELY(readOrder != nullptr)) {
                        
                        LOBOrder* writeSlot = LobOrderQueue->getNextWrite();
                        
                        if(LIKELY(writeSlot != nullptr)) {
                            // zero copy write directly to buffer
                            writeSlot->arrived_at = std::chrono::high_resolution_clock::now().time_since_epoch().count(); // readOrder has this as null arrived at is basically the time at which the order arrived on the gateway
                            writeSlot->system_id = GetOrAssignSystemId(readOrder->order_id, readOrder->req_type);
                            writeSlot->order_type = readOrder->order_type;
                            writeSlot->quantity = readOrder->quantity;
                            writeSlot->price = readOrder->price;
                            writeSlot->req_type = readOrder->req_type;
                            writeSlot->trader_id = readOrder->trader_id; // sniper is 0
                            
                            LobOrderQueue->updateWrite();
                            SniperOrderQueue->updateRead();
                        }
                    }

                    // take from market maker
                    readOrder = MMOrderQueue->getNextRead();
                    
                    if(LIKELY(readOrder != nullptr)) {
                        
                        LOBOrder* writeSlot = LobOrderQueue->getNextWrite();
                        
                        if(LIKELY(writeSlot != nullptr)) {
                            // zero copy write directly to buffer
                            writeSlot->arrived_at = std::chrono::high_resolution_clock::now().time_since_epoch().count();
                            writeSlot->system_id = GetOrAssignSystemId(readOrder->order_id, readOrder->req_type);
                            writeSlot->order_type = readOrder->order_type;
                            writeSlot->quantity = readOrder->quantity;
                            writeSlot->price = readOrder->price;
                            writeSlot->req_type = readOrder->req_type;
                            writeSlot->trader_id = readOrder->trader_id;
                            
                            LobOrderQueue->updateWrite();
                            MMOrderQueue->updateRead();
                        }
                    }

					// process acknowledgements 
                    LOBAcknowledgement* readAck = LobAckQueue->getNextRead();
                    
                    if(LIKELY(readAck != nullptr)) {
                        
                        // check who sent the order (sniper=0 or MM)
                        // change in architecture -------> acknowledgements will only be created and sent for Sniper, market maker is just responsible for filling in market traffic.
                        LFQueue<UserAcknowledgement>* targetQueue;
                        
                        if(readAck->traderId == 1) {

                            targetQueue = SniperAckQueue; 
                        
                            UserAcknowledgement* writeAck = targetQueue->getNextWrite();
                        
                            if(LIKELY(writeAck != nullptr)) {
                                writeAck->order_id = SystemToOrderId(readAck->system_id);
                                writeAck->timestamp = readAck->timestamp;
                                writeAck->filled_quantity = readAck->filled_quantity;
                                writeAck->price = readAck->price;
                                writeAck->status = readAck->status;
                                writeAck->side = readAck->side;

                                // always a good practice to commit first and then only update read unless you have a strong durability mechanism.
                                targetQueue->updateWrite();
                                LobAckQueue->updateRead();
                            }
                        } 
                    }
                }
            }
    };
}