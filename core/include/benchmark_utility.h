#pragma once 
#include<iostream>
#include<vector>
#include<algorithm>

namespace internal_lib {
	
	void showBench(std::string& bench_string, std::vector<uint64_t>& time_vector) noexcept {
		std::cout<<"================ BENCHMARK FOR : "<<bench_string<<" ================\n\n\n";

		std::sort(time_vector.begin(),time_vector.end());
		size_t siz = time_vector.size();

		std::cout<<" p50 : "<<time_vector[siz/2]<<"\n";
		std::cout<<" p75 : "<<time_vector[siz*0.75]<<"\n";
		std::cout<<" p90 : "<<time_vector[siz*0.9]<<"\n";
		std::cout<<" p99 : "<<time_vector[siz*0.99]<<"\n\n";

		std::cout<<"====================================================================\n\n\n";
		return ;
	}
}