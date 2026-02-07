#pragma once 
#include<iostream>
#include<vector>
#include<algorithm>
#include<chrono>
#include<x86intrin.h>

namespace internal_lib {

	// prevents compiler from reordering memory accesses across this point 
	inline void compiler_barrier() {
		asm volatile("" ::: "memory");
	}

	inline uint64_t now_cycles() {
		unsigned int aux;
		_mm_lfence();                      // drain all prior instructions
		uint64_t ts = __rdtscp(&aux);      // serializing read of TSC
		_mm_lfence();                      // prevent later instructions from moving before rdtscp
		return ts;
	}

	// returns cycles per nanosecond (e.g., ~4.5 for a 4.5 GHz CPU).
	inline double get_cycles_per_ns() {
		auto start_chrono = std::chrono::high_resolution_clock::now();
		uint64_t start_cyc = now_cycles();
		
		// spin for ~50ms to get a stable measurement
		while(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::high_resolution_clock::now() - start_chrono).count() < 50);
		
		uint64_t end_cyc = now_cycles();
		auto end_chrono = std::chrono::high_resolution_clock::now();
		
		double ns_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end_chrono - start_chrono).count();
		return (double)(end_cyc - start_cyc) / ns_elapsed;
	}

	void showBench(std::string& bench_string, std::vector<uint64_t>& time_vector, double cycles_per_ns) noexcept {
		
		std::cout<<"================ BENCHMARK FOR : "<<bench_string<<" ================\n\n\n";

		std::sort(time_vector.begin(),time_vector.end());
		size_t siz = time_vector.size();

		auto to_ns = [&](uint64_t cycles) -> uint64_t { return (uint64_t)(cycles / cycles_per_ns); };

		std::cout<<" p50 : "<<time_vector[siz/2]<<" cycles  ("<<to_ns(time_vector[siz/2])<<" ns)\n";
		std::cout<<" p75 : "<<time_vector[size_t(siz*0.75)]<<" cycles  ("<<to_ns(time_vector[size_t(siz*0.75)])<<" ns)\n";
		std::cout<<" p90 : "<<time_vector[size_t(siz*0.9)]<<" cycles  ("<<to_ns(time_vector[size_t(siz*0.9)])<<" ns)\n";
		std::cout<<" p99 : "<<time_vector[size_t(siz*0.99)]<<" cycles  ("<<to_ns(time_vector[size_t(siz*0.99)])<<" ns)\n\n";

		std::cout<<"====================================================================\n\n\n";
		return ;
	}
}