// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>

namespace common {

//
// Synchronization barrier.
//
class Barrier {
 public:
  explicit Barrier(size_t cnt) :
    threshold_(cnt), count_(cnt), generation_(0) { }

  // Wait until 'cnt' threads have reached the barrier.
  //
  // 'pre' is a void(size_t) function that is called just before the
  // thread enters the barrier.
  //
  // 'post' is a void(size_t) function that is called when the thread
  // exits the barrier. For the thread that releases the barrier
  // (count == 0), 'post' is called before the other threads are
  // released.
  //
  // Both are passed a size_t "count" argument that indicates how many
  // additional threads must reach the barrier after this one. Thus
  // "count" == 0 is passed if this thread is the last thread to reach
  // (and therefore release) the barrier.
  void
  Wait(std::function<void(size_t)>&& pre, std::function<void(size_t)>&& post) {
    std::unique_lock<std::mutex> lk(mu_);

    const size_t tgen = generation_;
    const size_t tcount = count_ - 1;

    pre(tcount);

    if (--count_ == 0) {
      post(tcount);
      generation_++;
      count_ = threshold_;
      cv_.notify_all();
    } else {
      cv_.wait(lk, [this, tgen] { return tgen != generation_; });
      post(tcount);
    }

  }

 private:
  std::mutex mu_;
  std::condition_variable cv_;
  const size_t threshold_;
  size_t count_;
  size_t generation_;
};

} // namespace common
