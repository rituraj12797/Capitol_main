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




	}
}