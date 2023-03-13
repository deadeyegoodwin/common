// Copyright 2023, David Goodwin. All rights reserved.

#include <exception>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include "random.h"
#include "task.h"

namespace {

#define Fail(MSG)                                                     \
  do {                                                                \
    std::cerr << "error: " << (MSG) << std::endl;                     \
    exit(1);                                                          \
  } while (false)

// Task that waits for a specified number of milliseconds and then
// increments an atomic.
class DelayTask : public common::Task {
public:
  DelayTask(const std::string& name, const std::chrono::milliseconds& duration, std::atomic<size_t>& inc) :
    name_(name), duration_(duration), inc_(inc) { }

  bool Invoke(uint64_t time_step) override {
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

// Task that safely pushes an identifier onto a shared vector.
class PushTask : public common::Task {
public:
  PushTask(const std::string& name, std::vector<int32_t>& vec, const int32_t id) :
    name_(name), vec_(vec), id_(id) { }

  bool Invoke(uint64_t time_step) {
    static std::mutex mu;
    std::lock_guard<std::mutex> lk(mu);
    vec_.push_back(id_);
    return false; // return ownership
  }

private:
  const std::string name_;
  std::vector<int32_t>& vec_;
  const int32_t id_;
};

} // namespace

// task_timewheel_pool_test <test number> <random seed>
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
    std::cerr << "usage: task_timewheel_pool_test <test number> [random-seed]" << std::endl;
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
        pool.Enqueue(std::move(delay_task), 1);
      }

      // When we enqueue while running, draining outcome is
      // unpredictable because of the timing between when some running
      // thread detects no pending tasks, when tasks are enqueued, and
      // when drain is requested. So we must keep training until all
      // tasks are complete.
      while (test_atomic != task_cnt) {
        pool.Drain();
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
        pool.Enqueue(std::move(delay_task), 1);
      }

      // When we enqueue while running, draining outcome is
      // unpredictable because of the timing between when some running
      // thread detects no pending tasks, when tasks are enqueued, and
      // when drain is requested. So we must keep training until all
      // tasks are complete.
      while (test_atomic != task_cnt) {
        pool.Drain();
      }
    }

    // Short task length, random time-delta exceeding bucket count
    if (test_number == 6) {
      const size_t task_cnt = 1024 * 1024;
      std::atomic<size_t> test_atomic { 0 };
      common::TaskTimewheelPool pool(worker_cnt, time_bucket_cnt);

      for (size_t i = 0; i < task_cnt; ++i) {
        auto delay_task = std::make_unique<DelayTask>(
          std::to_string(i), std::chrono::milliseconds(0), test_atomic);
        pool.Enqueue(std::move(delay_task), random(0, 3 * time_bucket_cnt));
      }
      pool.Drain();

      if (test_atomic != task_cnt) {
        Fail("test 6: atomic value (" + std::to_string(test_atomic.load()) + ") != task count (" +
             std::to_string(task_cnt) + ")");
      }
    }

    // Check that tasks invoked in time-delay order
    if (test_number == 7) {
      std::vector<int32_t> vec;
      common::TaskTimewheelPool pool(worker_cnt, time_bucket_cnt);
      std::unordered_map<int32_t, uint32_t> delays {
        { 0, 0 },
        { 1, 1 },
        { 2, 2 },
        { 3, time_bucket_cnt - 1 },
        { 4, time_bucket_cnt },
        { 5, time_bucket_cnt + 1 },
        { 6, (time_bucket_cnt * 2) - 1 },
        { 7, (time_bucket_cnt * 2) },
        { 8, (time_bucket_cnt * 2) + 1 }
      };
      for (const auto& pr : delays) {
        auto task = std::make_unique<PushTask>(std::to_string(pr.first), vec, pr.first);
        pool.Enqueue(std::move(task), pr.second);
      }
      pool.Drain();

      if (vec.size() != delays.size()) {
        Fail("test 7: vector size (" + std::to_string(vec.size()) + ") != delays (" +
             std::to_string(delays.size()) + ")");
      }

      for (size_t i = 0; i < delays.size(); ++i) {
        if (vec[i] != (int32_t)i) {
          Fail("test 7: vector[" + std::to_string(i) + "]=" + std::to_string(vec[i]) +
               ", expecting " + std::to_string(i));
        }
      }
    }

    // Multiple drains and pauses.
    if (test_number == 8) {
      const size_t task_cnt = 1024 * 1024;
      std::atomic<size_t> test_atomic { 0 };
      common::TaskTimewheelPool pool(worker_cnt, time_bucket_cnt);

      pool.Drain();
      pool.Pause();

      for (size_t i = 0; i < task_cnt; ++i) {
        auto delay_task = std::make_unique<DelayTask>(
          std::to_string(i), std::chrono::milliseconds(0), test_atomic);
        pool.Enqueue(std::move(delay_task), random(0, time_bucket_cnt - 1));
      }
      pool.Pause();
      pool.Drain();
      pool.Pause();
      pool.Drain();
      pool.Stop();

      if (test_atomic != task_cnt) {
        Fail("test 8: atomic value (" + std::to_string(test_atomic.load()) + ") != task count (" +
             std::to_string(task_cnt) + ")");
      }
    }

    // Control failure expected after stop.
    if (test_number == 9) {
      common::TaskTimewheelPool pool(worker_cnt, time_bucket_cnt);

      if (!pool.Stop()) { Fail("test 9: Stop() failed"); }
      if (pool.Pause()) { Fail("test 9: Pause() unexpected pass"); }
      if (pool.Drain()) { Fail("test 9: Drain() unexpected pass"); }
      if (pool.Run()) { Fail("test 9: Run() unexpected pass"); }
      if (!pool.Stop()) { Fail("test 9: Stop() failed"); }
    }

    // Run a pool to make sure initial pause -> running works
    // correctly.
    if (test_number == 10) {
      common::TaskTimewheelPool pool(worker_cnt, time_bucket_cnt);
      pool.Run();
    }

    // Run and drain a pool to make sure initial pause -> running ->
    // draining works correctly.
    if (test_number == 11) {
      common::TaskTimewheelPool pool(worker_cnt, time_bucket_cnt);
      pool.Run();
      pool.Drain();
    }

    if (test_number > 11) {
      Fail("unexpected test number " + std::to_string(test_number));
    }
  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << std::endl;
    exit(1);
  }

  return 0;
}
