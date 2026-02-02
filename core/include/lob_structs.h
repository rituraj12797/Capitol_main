 #pragma once

namespace internal_lib {

	// this is the main belly of market which will store data in sorted order/you can say this is the structure whihc will be on Buy and Sell side to store and match orders

	class LOBMatrix {


		size_t optimum_price; // this is the piinter which will point to the max Bid in Buy side and Min ask in sell side 
		vector<vector<internal_lib::LOBOrder>> store_; 
		vector<pair<int,int>> LUT; // look up table


		// to show that a particular order is dead we simply keep it's quantity as 0

		// hwo toi track the last alloctaf index so that we can knwo where to push next ??

		// we simply do not maintain any such index, all we do is reserver this row at start woth 1000 ---> this increases the capacity of our row to 1000, so meansing our vectopr 
		// can store till 1000 entries withotu resizing/malloc again ====> push back will still insert at the end ---> avoiding the need to keep sucn an index.

		// default constructor
		delete = default LOBMatrix(); // remove the other constructor like copy and all we will define this via a single constructor onlty snd that is 

		LOBMatrix(size_t max_price, size_t pp_queue_size) // 0-max price range and PerPrice queue size(the capacity of each row)

		void createOrder(LOBOrder& data) noexcept { // what i shapenning here is that this is being fetched from ring buffers, which is reading very fast, now if we do copying it into an object and then
			// creating a temp object here and then using this copy to create pobject in matrix will cause un-necessary latency in the system ----> so we simply construct it in place using placement new operator

			int price_index = data.price;

			// over rideable ----> for sell subclass LOBmatrix  this will do ====> if(optimal > price_index )---> optimal = price_index
			// over rideable ----> for buy subclass LOBmatrix  this will do ====> if(optimal < price_index )---> optimal = price_index

			store_[price_index].push_back(data);

			// updste the LUT

		}

		void updateOrder(LOBOrder& data) noexcept { // means this data already lives here just update its price/quantity

			// find the order from LUT.

			// priority ---> price based changes will take first then quantity based once 

			// price based changes 
			// ----> when price changes move it to that specific price_index. and update the price fiels 
			//  quantity based changes
			// ---> when quantity increase ----> mark the order dead and move it back to it's own vector and update the quantity field.
			// when quantity dicreases do nothing just update the quantity.

			// update the LUT.
		}

		void deleteOrder() noexcept {
			// simply mark the order dead and in the LUT, mark the posisiotn of this as -1, -1  
		}

	};

}