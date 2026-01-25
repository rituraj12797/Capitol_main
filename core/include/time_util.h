#pragma once
#include <chrono>
#include <ctime>

namespace internal_lib {
  typedef int64_t Nanos;

  // converters 
  constexpr Nanos NANOS_TO_MICROS = 1000;
  constexpr Nanos MICROS_TO_MILLIS = 1000;
  constexpr Nanos MILLIS_TO_SECS = 1000;
  constexpr Nanos NANOS_TO_MILLIS = NANOS_TO_MICROS * MICROS_TO_MILLIS;
  constexpr Nanos NANOS_TO_SECS = NANOS_TO_MILLIS * MILLIS_TO_SECS;

  // return the number of nanosceonds elapsed from system epoch
  inline auto getCurrentNanos() noexcept {
    return std::chrono::duration_cast <std::chrono::nanoseconds>(std::chrono::
        system_clock::now().time_since_epoch()).count();
  }

  // to get formated date ==> mostly we will be using this in  logging
  inline auto& getCurrentTimeStr(std::string* time_str) {
    const auto time = std::chrono::system_clock::
      to_time_t(std::chrono::system_clock::now());
    time_str->assign(ctime(&time));
    if(!time_str->empty())
      time_str->at(time_str->length()-1) = '\0';
    return *time_str;
  }
}