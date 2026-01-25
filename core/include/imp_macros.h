#pragma once

#include<iostream>  // for ctd::cerr
#include<string>  // for std::striing
#include<cstdlib> // for exist and EXIT_FAILURE



#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)


// ASSERT function simply check if the !cond == true ==> if condition is false then logs the message and exists with an error

// WHAT IS 'noexcept' ?? ==> noexcept is a keyword we append in the function description of a function this guraruntess the compielr that this function won't throw an exception. 

//   WHY IS THIS NEEDED ?? ==> OPTIMIZATION (HOW) => normally when function are called compilers has to generate hidden code for exception handling just in case if the program ever throws an exception, by explicitly adding this keyword in the function definition we guaruntee the compler that this won't throw an exception so the overhead associated with the exception handling is reduced leading to further improvements in performance, 

// What if the program still throws an exception ?? ==> in that case the compiler won't use destructor or cleanu the things, it will just terminate the program straight up.
namespace internal_lib {

    inline void ASSERT(bool cond, const std::string& msg) noexcept {
        if(UNLIKELY(!cond)) {
            std::cerr<<msg<<std::endl;
            exit(EXIT_FAILURE);
        }
    }


// FATAL function simply exists with a string message we must use this when we trigger a fatal scenario and server crashes.

    inline void FATAL(const std::string& msg) noexcept {
        std::cerr<<msg<<std::endl;
        exit(EXIT_FAILURE);
    }

}