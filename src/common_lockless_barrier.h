// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

#include <functional>

extern "C" {
#include "ck_barrier.h"
}

namespace common {

//
// Lockless synchronization barrier.
//
// Same API as common::Barrier so that the two classes can be used
// interchangeably.
//
// Patterned after ConcurrencyKit centralized barrier.
// (https://github.com/deadeyegoodwin/ck/blob/master/include/ck_barrier.h)
//
class LocklessBarrier {
 public:
  explicit LocklessBarrier(unsigned int cnt) :
    threshold_(cnt), count_(0), generation_(0) { }

  // Wait until 'threshold_' threads have reached the barrier.
  //
  // 'pre' is a void(size_t) function that is called just before the
  // thread enters the barrier. 'pre' is passed a size_t "count"
  // argument that indicates the order of this thread in reaching the
  // barrier. "count" indicates how many additional threads must reach
  // the barrier after this one. There is no ordering guarantee for
  // 'pre' function invocation, that is, pre(0) may be called before
  // pre(1), for example. It is possible that 'post' for thread A may
  // be called before 'pre' for thread B, but for a given thread,
  // 'pre' will always' be called before 'post'. 'pre' may be nullptr
  // to indicate that there is no "pre" function.
  //
  // 'post' is a void(size_t) function that is called when the thread
  // exits the barrier. 'post' is passed a size_t "count" argument
  // that indicates the order of this thread in reaching the
  // barrier. "count" indicates how many additional threads must reach
  // the barrier after this one. There is no ordering guarantee for
  // 'post' function invocation, except that the thread that releases
  // the barrier will call 'post' with count == 0 before any other
  // threads are released from the barrier. Thus it is guaranteed
  // that, 'post(0)' will be called before any other 'post'
  // call. 'post' may be nullptr to indicate that there is no "post"
  // function.
  void
  Wait(std::function<void(size_t)>&& pre, std::function<void(size_t)>&& post) {
    const uint64_t tgen = ck_pr_load_64(&generation_);
    const unsigned int count = ck_pr_faa_uint(&count_, 1);
    const size_t tcount = threshold_ - 1 - count;

    if (pre != nullptr) {
      pre(tcount);
    }

    if (tcount == 0) {
      if (post != nullptr) {
        post(tcount);
      }
      ck_pr_store_uint(&count_, 0);
      ck_pr_fence_memory();
      ck_pr_inc_64(&generation_);
    }
    else {
      ck_pr_fence_atomic_load();
      while (tgen == ck_pr_load_64(&generation_)) {
        ck_pr_stall();
      }
      if (post != nullptr) {
        post(tcount);
      }
    }

    ck_pr_fence_acquire();
  }

 private:
  const unsigned int threshold_;
  unsigned int count_;
  uint64_t generation_;
};

} // namespace common
