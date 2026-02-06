

#include "order_gateway_structs.h"

 #pragma once

// compiler hints for branch prediction
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

namespace internal_lib {

	// this is the main belly of market which will store data in sorted order/you can say this is the structure whihc will be on Buy and Sell side to store and match orders
	template <bool IsBuy>
	class LimitedOrderBook {

	private :


		size_t optimum_price; // this is the piinter which will point to the max Bid in Buy side and Min ask in sell side 
		std::vector<std::vector<internal_lib::LOBOrder>> store_; 
		std::vector<std::pair<int,int>> LUT; // look up table
		std::vector<int> active_counts;
		std::vector<internal_lib::LOBOrder> empty_price_level;
		size_t max_price_limit;


		void glideOptimum() noexcept {
			// this to use when MATRIX IS MODIFIED AND WE NEED 
			if(IsBuy) {
				// buy/bids side matrix 
				while(optimum_price > 0 && (active_counts[optimum_price] == 0)) { // no orders in this bids, try a lower bid
					optimum_price--;
				}

			} else {
				while(optimum_price < max_price_limit && (active_counts[optimum_price] == 0)) { // no orders in this bids, try a lower bid
					optimum_price++;
				}
			}
		}

		// to show that a particular order is dead we simply keep it's quantity as 0

		// hwo toi track the last alloctaf index so that we can knwo where to push next ??

		// we simply do not maintain any such index, all we do is reserver this row at start woth 1000 ---> this increases the capacity of our row to 1000, so meansing our vectopr 
		// can store till 1000 entries withotu resizing/malloc again ====> push back will still insert at the end ---> avoiding the need to keep sucn an index.

	public :

		// default constructor
		LimitedOrderBook() = delete; // remove the other constructor like copy and all we will define this via a single constructor onlty snd that is 

		LimitedOrderBook(size_t max_price_ticks, size_t max_entries_per_price) {
			// max_price_ticks range and PerPrice queue size(the capacity of each row)

			max_price_limit = max_price_ticks;

            store_.resize(max_price_ticks + 1); // reserve the store

            for(auto& row : store_) {
                row.reserve(max_entries_per_price);
            }

            
            active_counts.resize(max_price_ticks + 1, 0);
            LUT.resize(10000000, {-1, -1}); // 10 Million Systems Id's mapping

            // initialize optimum
            if (IsBuy) optimum_price = 0; 
            else optimum_price = max_price_ticks;

		}

		LOBOrder* peekLOBEntry(int systemId) noexcept {
			if(UNLIKELY(LUT[systemId].first == -1 || LUT[systemId].second == -1)) return nullptr; // if it does not exists in the LOB return null ptr.
			return &(store_[LUT[systemId].first][LUT[systemId].second]);
		}

		std::vector<internal_lib::LOBOrder>& getLevel(size_t best_ask_idx) noexcept { // returning a reference to a row of LOBORders
			if(best_ask_idx < store_.size()) {
				return store_[best_ask_idx];
			}
			// return a zero sized LOBOrder array 
			return empty_price_level;
		}


		void createOrder(LOBOrder& order) noexcept { // what i shapenning here is that this is being fetched from ring buffers, which is reading very fast, now if we do copying it into an object and then
			// creating a temp object here and then using this copy to create pobject in matrix will cause un-necessary latency in the system ----> so we simply construct it in place using placement new operator

			size_t price_index = static_cast<size_t>(order.price*10);

			// over rideable ----> for sell subclass LOBmatrix  this will do ====> if(optimal > price_index )---> optimal = price_index
			// over rideable ----> for buy subclass LOBmatrix  this will do ====> if(optimal < price_index )---> optimal = price_index

			// this will be virtual function ---> run time polymorphism will produce latency
			// to tackel this we use Templates --> LOBMatrix<IsBuy> 

			if (IsBuy) {
                if (price_index > optimum_price) optimum_price = price_index;
            } else {
                if (price_index < optimum_price) optimum_price = price_index;
            }

			// add a check for size here....
			store_[price_index].push_back(order);

			// update the LUT
			LUT[order.system_id] = {price_index,store_[price_index].size() - 1};
			// update active count
			active_counts[price_index]++;

			// no need to glide here the check is done above already in this function
			
		}

		void updateOrderQuantity(LOBOrder& data) noexcept { // means this data already lives here just update quantity
			// price based updates are handles by create and delete flows 


			// find the order from LUT.
			size_t price_row = LUT[data.system_id].first;
			size_t order_col = LUT[data.system_id].second;


			if(data.quantity != store_[price_row][order_col].quantity) {

				//  quantity based changes
				// ---> when quantity increase ----> mark the order dead and move it back to it's own vector and update the quantity field.

				if(data.quantity < store_[price_row][order_col].quantity) {
					// do nothign just update 
					store_[price_row][order_col].quantity = data.quantity;
				} else {
					store_[price_row][order_col].quantity = 0;
					store_[price_row].push_back(data);
					
					// update LUT
					LUT[data.system_id] = {price_row,store_[price_row].size() - 1};
				}
			}
		}

		void deleteOrder(int system_id) noexcept {

            if (UNLIKELY(system_id >= LUT.size())) return;

            int price_row = LUT[system_id].first;
            int order_col = LUT[system_id].second;

            if (LIKELY(price_row != -1)) {
                //  lazy delete (mark delete)
                store_[price_row][order_col].quantity = 0;
                
                // update LUT and active array
                LUT[system_id] = {-1, -1};
                active_counts[price_row]--;

                // only expensive glide if we emptied the BEST price level
                if (UNLIKELY(active_counts[price_row] == 0 && price_row == (int)optimum_price)) {
                    glideOptimum(); 
                }
            }
        }

		size_t getOptimumPriceIndex() noexcept {
			return optimum_price;
		}

	};

	struct BroadcastElement {
        int system_id;    // Reference to the order in the book
        float price;         // Trade Price
        int quantity;     // AMOUNT TRADED (if type=='T') or NEW BALANCE (if type=='U') or FULL SIZE (if type=='A')
        char side;            // 'B'uy or 'S'ell
        char type;            // 'A'dd, 'U'pdate, 'D'elete, 'T'rade
    };


}