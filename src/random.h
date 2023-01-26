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
  Random() : Random(rdev_()) { }
  Random(int32_t seed) : seed_(seed), generator_(seed) { }

  int32_t Seed() const { return seed_; }

  // Returns a random integer in the range [min, max], inclusive.
  int32_t operator()(const int32_t min, const int32_t max) {
    std::uniform_int_distribution<int32_t> distribution(min, max);
    return distribution(generator_);
  }

private:
  static std::random_device rdev_;
  const int32_t seed_;
  std::mt19937 generator_;
};

} // namespace common
