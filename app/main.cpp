#pragma once 

#include "lf_queue.h"
#include "lob_structs.h"
#include "order_gateway_structs.h"
#include "thread_utils.h"

#include "../core/src/alpha_tester.cpp"
#include "../core/src/order_gateway.cpp"
#include "../core/src/matching_engine.cpp"

int main() {

	// each lfqueue defined with 5M size 

	internal_lib::LFQueue<internal_lib::UserOrder> soq(1000000); // Sniper Order Queue
	internal_lib::LFQueue<internal_lib::UserAcknowledgement> saq(1000000); // Sniper Acknoweldgement Queue
	internal_lib::LFQueue<internal_lib::LOBOrder> loq(1000000); // LOB Order queue
	internal_lib::LFQueue<internal_lib::LOBAcknowledgement> laq(1000000); // LOB Acknowledgement Queue
	internal_lib::LFQueue<internal_lib::BroadcastElement> bq(1000000); // broadcast queue

	// a lf queue to denote one strem from market maker but since we have not written market maker right now we won't fill anything yet.
	internal_lib::LFQueue<internal_lib::UserOrder> mmoq(100); // market maker order queue





	// define ME
	internal_lib::MatchingEngine matchingEngine(10000,400,&loq,&laq,&bq);

	// define OG
	internal_lib::OrderGateway orderGateway(&laq, &soq, &saq, &mmoq, &loq);

	// define alpha
	internal_lib::AlphaServer alphaServer(&soq,&saq,&bq);


	// create atomic variables for these components to run and terminate on 


	std::atomic<bool> start_matching_engine = {false};
	std::atomic<bool> terminate_matching_engine = {false};

	std::atomic<bool> start_ordergate_way = {false};
	std::atomic<bool> terminate_ordergate_way = {false};

	std::atomic<bool> start_alpha_server = {false};
	std::atomic<bool> terminate_alpha_server = {false};



	// create threads for each 

	auto matching_engine_thread = internal_lib::createAndStartThread(1, "Matching Engine", [&](){ 
        matchingEngine.matchingEngineLoop(start_matching_engine, terminate_matching_engine); 
    });
    
    auto order_gateway_thread = internal_lib::createAndStartThread(2, "Order Gateway", [&](){ 
        orderGateway.run(start_ordergate_way, terminate_ordergate_way); 
    });

    auto alpha_server_thread = internal_lib::createAndStartThread(3, "Alpha Server", [&](){ 
        alphaServer.AlphaRun(start_alpha_server, terminate_alpha_server); 
    });

	// wait for 3 seconds to letthe cpu reach max freq,
    std::this_thread::sleep_for(std::chrono::seconds(3));
	// set start = true one by one

	// start ME, start OG then start AlphaServer
	std::cout<<"~~~~~~~~~~~~~~~~~~~~~ CAPITOL STARTED ~~~~~~~~~~~~~~~~~~~~~~~~~~~~` "<<"\n";
	start_matching_engine.store(true);
	std::cout<<" Matching Engine started \n";
	start_ordergate_way.store(true);
	std::cout<<" Order Gateway started \n";
	start_alpha_server.store(true);
	std::cout<<" Alpha Server started \n";


	// stop now 
	//  wait for 3 seconds
	std::this_thread::sleep_for(std::chrono::seconds(5));

	// stop Alpha 
	std::cout<<" Terminating Alpha \n";
	terminate_alpha_server.store(true);
	std::cout<<" Terminating Order Gateway \n";
	terminate_ordergate_way.store(true);
	std::cout<<" Terminating Matching Engine \n";
	terminate_matching_engine.store(true);
	


	// join threads now
	matching_engine_thread->join();
	order_gateway_thread->join();
	alpha_server_thread->join();


	delete matching_engine_thread;
	delete order_gateway_thread;
	delete alpha_server_thread;

	std::cout<<"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ CLOSING CAPITOL ~~~~~~~~~~~~~~~~~~ \n";

	return 0;

}
