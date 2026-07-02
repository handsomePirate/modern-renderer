#pragma once

#include <chrono>

inline auto start = std::chrono::high_resolution_clock::now();
inline float getTimeSeconds() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::high_resolution_clock::now() - start)
             .count() *
         0.000001f;
}
