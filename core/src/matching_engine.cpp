#pragma once 

#include "lf_queue.h"
#include "lob_structs.h"

#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

namespace internal_lib {
	class MatchingEngine {


		// dependency injection here too====> comms/lfqueues will be passed as a reference here, they will be defined in main and referenced here.

		private : 

		internal_lib::LFQueue<internal_lib::LOBOrder>* LobOrderQueue; 
        internal_lib::LFQueue<internal_lib::LOBAcknowledgement>* LobAckQueue; 

        internal_lib::LFQueue<internal_lib::BroadcastElement>* BroadcastQueue; // we keep it only incremental, as snapshotting is cmplex logic.
        // need to create this structure in lob_structs.h


        internal_lib::LimitedOrderBook<true> BuyOrderBook; // it has it's Own LUT
        internal_lib::LimitedOrderBook<false> SellOrderBook; // it has it's own LUT

        public : 

        MatchingEngine() = delete;

        MatchingEngine(
        	size_t max_price_ticks,
        	size_t max_entries_per_price,
        	LFQueue<internal_lib::LOBOrder>* req_q,
        	LFQueue<internal_lib::LOBOrder>* ack_q,
        	LFQueue<internal_lib::LOBOrder>* brdcst_q
        ) : LobOrderQueue(req_q),
        	LobAckQueue(ack_q),
        	BroadcastQueue(brdcst_q),

        	BuyOrderBook(max_price_ticks, max_entries_per_price),
        	SellOrderBook(max_price_ticks, max_entries_per_price)
        	{} // empty body 


        void matchingEngineLoop() {

        	while(run) { // this will run the readOrder again and again ------> the architecture here is event based.
        		readOrder();
        	}

        }

        void readOrder() noexcept { // this will read from queue
        	// step 1 
        	// read from LobOrderQueue

        	LOBOrder* order = LobOrderQueue->getNextread(); // i would say I need to do somethign such that we only maintain a pointer and do not copy the order, since the read head wont move 
        	// unless we call it to... we can reference it at will, and hence we needs not to maintain a copy we can just use it as reference as long we want.


        	if(UNLIKELY(order == nullptr)) {
        		// return.
        		return ;
        	} 

        	bool is_buy = ((order->order_type == 'b') ? true : false);
        	if(order->req_type == 'c') {
        		// call createOrderHandler
        		createOrderHandler(*order, is_buy); // pass by reference 
        	} else if(order->req_type == 'u') {
        		// call updateOrderHandler
        		updateHandler(*order, is_buy);
        	} else {
        		// call delete orderHandler
        		deleteHandler(*order, is_buy);
        	}

        	// now update the read. 
        	LobOrderQueue->updateRead();

        }

        void createOrderHandler(LOBOrder& order, bool is_buy) noexcept {


        	// after aggressive check if quantity is still > 0 ---> (due to no or partial matching)

        	aggressiveMatch(order, is_buy);

        	// add to LOB and transfer this even to Logger
        	if(order.quantity > 0) {
        		if(is_buy) {
        			BuyOrderBook.createOrder(order);
        		} else {
	         		SellOrderBook.createOrder(order);
        		}
        		// send acknowledge to Ordergateway. for creating 
        		// write code here 
        	}
        }

        void updateHandler(LOBOrder& order, bool is_buy) noexcept {

        	// if this makes a price updatethen we do delete and the call craetOrderhandler it Will automatically do aggressive checking fpor us no need to qrite separate code for that
        	// but if quanrtity related changes then call the update function from lob_structs class

        	// to peek entry from the LOB.
        	LOBOrder* order_entry_in_lob;

        	if(is_buy) {
        		order_entry_in_lob = BuyOrderBook.peekLOBEntry(order.system_id);
        	} else {
        		order_entry_in_lob = SellOrderBook.peekLOBEntry(order.system_id);
        	}

        	if (UNLIKELY(order_entry_in_lob == nullptr)) return;  // safety check 

    		bool quantity_change = (order_entry_in_lob->quantity != order.quantity);
    		bool price_change = (order_entry_in_lob->price != order.price);


        	if(price_change) {
        		// price based difference, do delete and update
        		// since order_entry was a pointer we need to pass the reference in deleteHandler so use asterisk
        		deleteHandler(*order_entry_in_lob, is_buy);

        		// by default we create new order so need not to update the orde separately
        		createOrderHandler(order,is_buy);

        		// won't send acknowledgement now, as delete and create form here would already have sent a succesfull one.
        	} else if(quantity_change) {
        		// if only quantity changes 
        		if(is_buy) { 
        			BuyOrderBook.updateOrderQuantity(order);
        		} else {
        			SellOrderBook.updateOrderQuantity(order);
        		}
        	}

        	// LOG
        }

        void deleteHandler(LOBOrder& order, bool is_buy) noexcept {
        	// call the LOB delete handler

        	if(is_buy) {
        		BuyOrderBook.deleteOrder(order.system_id);

        	} else {
        		SellOrderBook.deleteOrder(order.system_id);
        	}
        	// send acknowledgement 


        	// LOG
        }

        void aggressiveMatch(LOBOrder& order, bool is_buy) noexcept { 

        	// if order of type sell
        	// attack buy side 

        	// Loop untill orders are matching and modify LOB

        	// if matching ------> 

        	// if full ---> aggressive bid/ask quantity == passive optimal ask/bid quantity ----> remove the passive entry modify LOB using member functions from lob_structs

        	// if partial ---> 2 types
        	// Loop here untill nothing more matches  ----> whenerv matches send acknowledge to orderGateWay
        	// type 1 partial : aggressive bid/ask quantity < passive optimal ask/bid quantity ----> subtract the (aggressive quantity) from passive optimal order.
        	// type 2 partial : aggressive bid/ask quantity > passive optimal ask/bid quantity ----> subtract the (passive quantity) from active order, and add the updated aggressive order with new quantity to LOB


        	// not matching
        	// add remaining order in LOB
        	
        	// exit
            // if any aggressive match happens then this will return true else it will return false


        	// IN AGGRESSIVE MATCHING DO ONE MORE THING IF 2 ORDERS MATCH, BE SURE TO CHECK IF THEIR TRADER_ID is not same 
            if (is_buy) {
                // if order of type buy
                // attack sell side 
                
                // Loop untill orders are matching and modify LOB
                while(order.quantity > 0) {
                    
                    // Check spread: Buy Price >= Best Ask
                    size_t best_ask_idx = SellOrderBook.getOptimumPriceIndex();
                    size_t bid_price_idx = static_cast<size_t>(order.price * 10);
                    
                    if (bid_price_idx < best_ask_idx) break; // Spread not crossed

                    auto& level = SellOrderBook.getLevel(best_ask_idx);
                    if (level.empty()) break; 

                    bool wash_trade_match = false;


                    for (auto& passive : level) {
                        // if matching ------> 
                        if (order.quantity == 0) break;
                        if (UNLIKELY(passive.quantity == 0)) continue; // Skip dead orders

                        // can match now --->  check for wash trading ----> 
                        if(UNLIKELY(passive.trader_id == order.trader_id)) {
                            wash_trade_match = true;
                            break; // get out we will settle this in LOB now
                        }
                        
                        int trade_qty = (order.quantity < passive.quantity) ? order.quantity : passive.quantity;
                        double trade_price = passive.price;

                        // type 1 partial / type 2 partial logic combined via subtraction:
                        // subtract the (aggressive quantity) from passive optimal order.
                        // subtract the (passive quantity) from active order
                        order.quantity -= trade_qty;
                        passive.quantity -= trade_qty;

                        // broadcast change
                        sendIncrementalChange(passive.system_id, trade_price, trade_qty, 'T', 'S');

                        // acknowledge back
                        acknowledgeBackToOrderGateway(order.system_id, trade_price, trade_qty, 'T', 'B'); // Aggressor
                        acknowledgeBackToOrderGateway(passive.system_id, trade_price, trade_qty, 'T', 'S'); // Passive


                        // if full ---> aggressive bid/ask quantity == passive optimal ask/bid quantity 
                        // remove the passive entry modify LOB using member functions from lob_structs
                        if (passive.quantity == 0) {
                             SellOrderBook.deleteOrder(passive.system_id);
                        }
                    }

                    if(UNLIKELY(wash_trade_match)) {
                        break; // get out from the loop
                    }
                }
            } else {
                // if order of type sell
                // attack buy side 
                
                // Loop untill orders are matching and modify LOB
                while(order.quantity > 0) {
                    
                    size_t best_bid_idx = BuyOrderBook.getOptimumPriceIndex();
                    size_t ask_price_idx = static_cast<size_t>(order.price * 10);

                    if (ask_price_idx > best_bid_idx) break;

                    auto& level = BuyOrderBook.getLevel(best_bid_idx);
                    if (level.empty()) break;

                    bool wash_trade_match = false;

                    for (auto& passive : level) {
                        // if matching ------> 
                        if (order.quantity == 0) break;
                        if (UNLIKELY(passive.quantity == 0)) continue;


                        if(UNLIKELY(passive.trader_id == order.trader_id)) {
                            wash_trade_match = true;
                            break; // get out we will settle this in LOB now
                        }

                        int trade_qty = (order.quantity < passive.quantity) ? order.quantity : passive.quantity;
                        double trade_price = passive.price;

                        // subtract the (aggressive quantity) from passive optimal order.
                        // subtract the (passive quantity) from active order
                        order.quantity -= trade_qty;
                        passive.quantity -= trade_qty;

                        // whenerv matches send acknowledge to orderGateWay
                        acknowledgeBackToOrderGateway(order.system_id, trade_price, trade_qty, 'T', 'S');
                        acknowledgeBackToOrderGateway(passive.system_id, trade_price, trade_qty, 'T', 'B');
                        
                        sendIncrementalChange(passive.system_id, trade_price, trade_qty, 'T', 'B');

                        // remove the passive entry modify LOB
                        if (passive.quantity == 0) {
                             BuyOrderBook.deleteOrder(passive.system_id);
                        }
                    }

                    if(UNLIKELY(wash_trade_match)) {
                        break; // get out from the loop
                    }

                }
            }
            return ;
        }


        void acknowledgeBackToOrderGateway(int sys_id, double px, int qty, char status, char side) noexcept {
            // .. will receive soem ack object and send this to the ackonledge queue 
            internal_lib::LOBAcknowledgement ack;
            ack.system_id = sys_id;
            ack.price = px;
            ack.quantity = qty;
            ack.status = status;
            ack.side = side;
            
            // write to AckQueue.
            LOBAcknowledgement* write_obj = LobAckQueue->getNextWrite();

            // I know this is risk we should keep somethign likea busy wait or a spoin wait here
            // but since our queues is large (5x the crash size) this is high probvablity that it will not block,
            // also we doing testing so need to remove the busy wait to a simple check. 


            /*
             while(write_obj == nullptr) { 
                write_obj = LobAckQueue->getNextWrite();
            }*/
            if(write_obj == nullptr) { 
            	write_obj = LobAckQueue->getNextWrite();
            }

            // now it is a writeable position 
            // the entry inside the adress pointed by write_obj be set as ack
           *write_obj = ack; // write done

            LobAckQueue->updateWrite(); // updates write position.
        }

        void sendIncrementalChange(int sys_id, double px, int qty, char type, char side) noexcept {
            // will get some incremental change and write it to market data puiblisher queue
            internal_lib::BroadcastElement be;
            be.system_id = sys_id;
            be.price = px;
            be.quantity = qty;
            be.type = type;
            be.side = side;
            
            // write to marketDataBroadcasterQueue

            BroadcastElement* write_obj = BroadcastQueue->getNextWrite();

           // I know this is risk we should keep somethign likea busy wait or a spoin wait here
            // but since our queues is large (5x the crash size) this is high probvablity that it will not block,
            // also we doing testing so need to remove the busy wait to a simple check. 


            /*
             while(write_obj == nullptr) { 
                write_obj = LobAckQueue->getNextWrite();
            }*/
            if(write_obj == nullptr) { 
                write_obj = BroadcastQueue->getNextWrite();
            }


            // can now write to this point 
            *write_obj = be;

            BroadcastQueue->updateWrite(); // update write index now 


        }

        void writeToLogger() noexcept {
        	// wil get some event and write it to logger.
        }

	}
}

