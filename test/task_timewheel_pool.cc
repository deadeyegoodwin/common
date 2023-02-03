// Copyright 2023, David Goodwin. All rights reserved.

#include <exception>
#include <chrono>
#include <iostream>
#include <thread>
#include "random.h"
#include "task.h"

namespace {

#define Fail(MSG)                                                     \
  do {                                                                \
    std::cerr << "error: " << (MSG) << std::endl;                     \
    exit(1);                                                          \
  } while (false)

// Task that waits for a specified number of milliseconds and then
// increments and atomic.
class DelayTask : public common::Task {
public:
  DelayTask(const std::string& name, const std::chrono::milliseconds& duration, std::atomic<size_t>& inc) :
    name_(name), duration_(duration), inc_(inc) { }

  bool Invoke() {
//    std::cerr << "invoked " << name_ << " on " << std::this_thread::get_id() << std::endl;
    if (duration_.count() > 0) {
      std::this_thread::sleep_for(duration_);
    }
    ++inc_;
    return false; // return ownership
  }

private:
  const std::string name_;
  const std::chrono::milliseconds duration_;
  std::atomic<size_t>& inc_;
};

} // namespace

// task_timewheel_pool <test number> <random seed>
int
main(int argc, char **argv)
{
  const size_t worker_cnt = 8;
  const size_t time_bucket_cnt = 128;

  // Echo command line
  for (auto i = 0; i < argc; ++i) {
    std::cout << argv[i] << " ";
  }
  std::cout << std::endl;

  if (argc < 2) {
    std::cerr << "usage: task_timewheel_pool <test number> [random-seed]" << std::endl;
    exit(1);
  }

  int32_t test_number = std::stoi(argv[1]);
  int32_t random_seed = 0;
  if (argc >= 3) {
    random_seed = std::stoi(argv[2]);
  }

  common::Random random(random_seed);
  std::cout << "random-seed: " << random.Seed() << std::endl;

  try {

    // Destroy a paused pool
    if (test_number == 0) {
      common::TaskTimewheelPool pool(worker_cnt, time_bucket_cnt);
    }

    // Short task length, random time-delta.
    if (test_number == 1) {
      const size_t task_cnt = 1024 * 1024;
      std::atomic<size_t> test_atomic { 0 };
      common::TaskTimewheelPool pool(worker_cnt, time_bucket_cnt);

      for (size_t i = 0; i < task_cnt; ++i) {
        auto delay_task = std::make_unique<DelayTask>(
          std::to_string(i), std::chrono::milliseconds(0), test_atomic);
        pool.Enqueue(std::move(delay_task), random(0, time_bucket_cnt - 1));
      }
      pool.Drain();

      if (test_atomic != task_cnt) {
        Fail("test 1: atomic value (" + std::to_string(test_atomic.load()) + ") != task count (" +
             std::to_string(task_cnt) + ")");
      }
    }

    // Random task length, random time-delta.
    if (test_number == 2) {
      const size_t task_cnt = 1024 * 2;
      std::atomic<size_t> test_atomic { 0 };
      common::TaskTimewheelPool pool(worker_cnt, time_bucket_cnt);

      for (size_t i = 0; i < task_cnt; ++i) {
        auto delay_task = std::make_unique<DelayTask>(
          std::to_string(i), std::chrono::milliseconds(random(0, 20)), test_atomic);
        pool.Enqueue(std::move(delay_task), random(0, time_bucket_cnt - 1));
      }
      pool.Drain();

      if (test_atomic != task_cnt) {
        Fail("test 2: atomic value (" + std::to_string(test_atomic.load()) + ") != task count (" +
             std::to_string(task_cnt) + ")");
      }
    }

    // Short task length, zero time-delta.
    if (test_number == 3) {
      const size_t task_cnt = 1024 * 1024;
      std::atomic<size_t> test_atomic { 0 };
      common::TaskTimewheelPool pool(worker_cnt, time_bucket_cnt);

      for (size_t i = 0; i < task_cnt; ++i) {
        auto delay_task = std::make_unique<DelayTask>(
          std::to_string(i), std::chrono::milliseconds(0), test_atomic);
        pool.Enqueue(std::move(delay_task), 0);
      }
      pool.Drain();

      if (test_atomic != task_cnt) {
        Fail("test 3: atomic value (" + std::to_string(test_atomic.load()) + ") != task count (" +
             std::to_string(task_cnt) + ")");
      }
    }

    // Long task length, enqueue while running.
    if (test_number == 4) {
      const size_t task_cnt = 16;
      std::atomic<size_t> test_atomic { 0 };
      common::TaskTimewheelPool pool(worker_cnt, time_bucket_cnt);

      pool.Run();
      for (size_t i = 0; i < task_cnt; ++i) {
        auto delay_task = std::make_unique<DelayTask>(
          std::to_string(i), std::chrono::milliseconds(2 * 1000), test_atomic);
        pool.Enqueue(std::move(delay_task), 0);
      }
      pool.Drain();

      if (test_atomic != task_cnt) {
        Fail("test 4: atomic value (" + std::to_string(test_atomic.load()) + ") != task count (" +
             std::to_string(task_cnt) + ")");
      }
    }

    // Short task length, enqueue while running.
    if (test_number == 5) {
      const size_t task_cnt = 1024 * 1024;
      std::atomic<size_t> test_atomic { 0 };
      common::TaskTimewheelPool pool(worker_cnt, time_bucket_cnt);

      pool.Run();
      for (size_t i = 0; i < task_cnt; ++i) {
        auto delay_task = std::make_unique<DelayTask>(
          std::to_string(i), std::chrono::milliseconds(0), test_atomic);
        pool.Enqueue(std::move(delay_task), 0);
      }
      pool.Drain();

      if (test_atomic != task_cnt) {
        Fail("test 5: atomic value (" + std::to_string(test_atomic.load()) + ") != task count (" +
             std::to_string(task_cnt) + ")");
      }
    }

    if (test_number > 5) {
      Fail("unexpected test number " + std::to_string(test_number));
    }
  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << std::endl;
    exit(1);
  }

  return 0;
}
