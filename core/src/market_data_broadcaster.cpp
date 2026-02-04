// this is the component which will blast the changes happening in LOB to alpha_tester and market maker.
#pragma once 

namespace internal_lib {


	class MarketDataBroadcaster {

		private : 
			internal_lib::LFQueue<internal_lib::BroadcastElement>* BroadcastQueue; // read incremental changes for our LOB from matching engine

			internal_lib::LFQueue<internal_lib::BroadcastElement>* BroadcastToMarketMaker; // blast incremental changes to market maker ----> we may not need this later on
			// as the role of market maker is only to blast market data to us it wont't do this based on our LOB.

			internal_lib::LFQueue<internal_lib::BroadcastElement>* BroadcastToAlphaEngine; // blast incremental changes to our alpha engine.

		public :

			// a simple function which will be called inside the createAndStart function for this thread whihc will read from BroadcastQueue and send them to BroadcastToMarketMaker and BroadcastToAlphaEngine
			void BroadcastIncrementalChanges() noexcept {

				 int run = 1;

				while(true) {

					run  = 0;
					// read from BroadCastQueue
					int overall_data_transferred = 1;
					*BroadcastElement incrementalChange = BroadcastQueue->getNextRead();

					// abn entry 
					if(incrementalChange != nullptr ){
						// send em

						*BroadcastElement writeIncrementalChangeToMarketMaker = BroadcastToMarketMaker->getNextWrite();
						*BroadcastElement writeIncrementalChangeToAlphaEngine = BroadcastToAlphaEngine->getNextWrite();

						if(writeIncrementalChangeToMarketMaker != nullptr && writeIncrementalChangeToAlphaEngine != nullptr) {
							*writeIncrementalChangeToMarketMaker = *incrementalChange;
							*writeIncrementalChangeToAlphaEngine = *incrementalChange;

							// first commit the updatetion by calling the update write function to succesfully incrementing tha data.
							BroadcastToMarketMaker->updateWrite();
							BroadcastToAlphaEngine->updateWrite();

							// now updatethe read pointer
							BroadcastQueue->updateRead();


						} else {
							overall_data_transferred &= 0;
						}


					} else {
						overall_data_transferred &= 0;
					}

					run |= overall_data_transferred;
					// yield here if run is 0 
				}
			}

	}

}