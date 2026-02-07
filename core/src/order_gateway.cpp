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

            internal_lib::SIMDBPlusTree<long long, int, 256> BPTree; 
            std::vector<long long> LUT; 
            
            int next_system_id = 0; // start from 0


            // 
            int orders_received = 0;
            int LOB_orders_sent = 0;

        public :

            OrderGateway(
                     LFQueue<internal_lib::LOBAcknowledgement>* laq,  
                     LFQueue<internal_lib::UserOrder>* soq, 
                     LFQueue<internal_lib::UserAcknowledgement>* saq, 
                     LFQueue<internal_lib::UserOrder>* mmoq, 
                     LFQueue<internal_lib::LOBOrder>* loq) 
                    : 
                     LobAckQueue(laq),
                     SniperOrderQueue(soq),
                     SniperAckQueue(saq),
                     MMOrderQueue(mmoq),
                     LobOrderQueue(loq)
                      {
                // initialize B+ Tree
                internal_lib::SIMDBPlusTree<long long, int, 256>::init(100000);
                
                // Pre-allocate LUT
                LUT.resize(1000000);
            }

            long long SystemToOrderId(int sysId) noexcept {
                if(LIKELY(sysId < LUT.size()))return LUT[sysId];
                return -1; // sysId > size return -1;
                
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

            void run(
                     std::atomic<bool>& start_order_gateway,
                     std::atomic<bool>& terminate_order_gateway                    
                     ) noexcept { 

                // update local pointers
                

                // press the accelerator but hold the breaks untill we receive a signal from main to blast off!!!
                                std::cout<<" ============= running ==========\n ";

                while(!start_order_gateway.load(std::memory_order_acquire)){
                    if(terminate_order_gateway.load(std::memory_order_acquire)) return;
                }

                // NOW !!!!!!!!!!!!
                while(!terminate_order_gateway.load(std::memory_order_acquire)){
                    
                        // take input from sniper
                        UserOrder* readOrder = SniperOrderQueue->getNextRead(); 

                        if(LIKELY(readOrder != nullptr)) {

                            // testing
                            orders_received++;


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

                                LOB_orders_sent++;
                            }
                        }

                        // take from market maker ---> we will only define a queue as of now for market maker but nothign will be there as of now 
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

                        // if(readAck->traderId == 1) { // we will publish for Alpha engine so we can copmment out this if 

                            targetQueue = SniperAckQueue; 
                        
                            UserAcknowledgement* writeAck = targetQueue->getNextWrite();
                        
                            if(LIKELY(writeAck != nullptr)) {
                                writeAck->order_id = SystemToOrderId(readAck->system_id);
                                writeAck->quantity = readAck->quantity;
                                writeAck->price = readAck->price;
                                writeAck->status = readAck->status;
                                writeAck->side = readAck->side;

                                // always a good practice to commit first and then only update read unless you have a strong durability mechanism.
                                targetQueue->updateWrite();
                                LobAckQueue->updateRead();
                            }
                        // } 
                        }
                    
                }

                std::cout<<" User orders received : "<<orders_received<<"\n" ;
                std::cout<<" LOB Orders sent : "<<LOB_orders_sent<<"\n";

                return ;

                
            }
    };
}