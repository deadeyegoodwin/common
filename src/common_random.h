// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

#include <stdint.h>
#include <random>

namespace common {

//
// Random
//
class Random {
public:
  // With no argument or 0 (zero) argument, the random generator is
  // initialized with a random seed.
  Random(int32_t seed=0) : seed_((seed == 0) ? int32_t(rdev_()) : seed), generator_(uint32_t(seed_)) { }

  int32_t Seed() const { return seed_; }

  // Returns a random int32_t in the range [min, max], inclusive.
  int32_t operator()(const int32_t min, const int32_t max) {
    std::uniform_int_distribution<int32_t> distribution(min, max);
    return distribution(generator_);
  }

  // Returns a random uint32_t in the range [min, max], inclusive.
  uint32_t operator()(const uint32_t min, const uint32_t max) {
    std::uniform_int_distribution<uint32_t> distribution(min, max);
    return distribution(generator_);
  }

  // Returns a random int64_t in the range [min, max], inclusive.
  int64_t operator()(const int64_t min, const int64_t max) {
    std::uniform_int_distribution<int64_t> distribution(min, max);
    return distribution(generator_);
  }

  // Returns a random uint64_t in the range [min, max], inclusive.
  uint64_t operator()(const uint64_t min, const uint64_t max) {
    std::uniform_int_distribution<uint64_t> distribution(min, max);
    return distribution(generator_);
  }

private:
  static std::random_device rdev_;
  const int32_t seed_;
  std::mt19937 generator_;
};

} // namespace common
