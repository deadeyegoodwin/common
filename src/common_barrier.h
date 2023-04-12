// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>

namespace common {

//
// Synchronization barrier.
//
// Same API as common::LocklessBarrier so that the two classes can be
// used interchangeably.
//
class Barrier {
 public:
  explicit Barrier(unsigned int cnt) :
    threshold_(cnt), count_(cnt), generation_(0) { }

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
    std::unique_lock<std::mutex> lk(mu_);

    const size_t tgen = generation_;
    const size_t tcount = count_ - 1;

    if (pre != nullptr) {
      pre(tcount);
    }

    if (--count_ == 0) {
      if (post != nullptr) {
        post(tcount);
      }
      generation_++;
      count_ = threshold_;
      cv_.notify_all();
    } else {
      cv_.wait(lk, [this, tgen] { return tgen != generation_; });
      if (post != nullptr) {
        post(tcount);
      }
    }
  }

 private:
  std::mutex mu_;
  std::condition_variable cv_;
  const unsigned int threshold_;
  unsigned int count_;
  size_t generation_;
};

} // namespace common
