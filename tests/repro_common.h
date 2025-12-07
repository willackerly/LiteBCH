#ifndef REPRO_COMMON_H
#define REPRO_COMMON_H

#include <cstdint>
#include <vector>

// Simple Linear Congruential Generator
// X_{n+1} = (a * X_n + c) % m
// Using standard glibc parameters:
// a = 1103515245
// c = 12345
// m = 2^31

struct SimpleLCG {
  std::uint32_t state;

  SimpleLCG(std::uint32_t seed) : state(seed) {}

  std::uint32_t next() {
    state = (1103515245 * state + 12345) & 0x7FFFFFFF;
    return state;
  }

  // Get a random bit (0 or 1)
  int next_bit() { return next() % 2; }
};

// Simple DJB2-like checksum for vector<int> (0/1)
std::uint32_t calculate_checksum(const std::vector<int> &data) {
  std::uint32_t hash = 5381;
  for (int val : data) {
    hash = ((hash << 5) + hash) + val; /* hash * 33 + val */
  }
  return hash;
}

#endif
