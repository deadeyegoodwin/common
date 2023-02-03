// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <deque>
#include <memory>
#include <thread>
#include <vector>
#include "error.h"

namespace common {

//
// Task
//
// Base class for all task objects.
//
class Task {
public:
  virtual void TaskInvoke() = 0;
};

//
// TaskPool
//
// A thread pool that executes enqueued tasks and then returns the
// completed tasks on another queue. A task object must be wrapped in
// a std::unique_ptr for enqueuing/dequeuing so that ownership is
// clearly passed to/from the task queue. The task must derive from
// class Task.
//
class TaskPool {
public:
  TaskPool(const size_t worker_cnt);
  ~TaskPool();

  size_t InFlightTaskCount() const;
  void Enqueue(std::unique_ptr<Task> task);
  bool Dequeue(std::unique_ptr<Task>& task);
  bool Dequeue(std::unique_ptr<Task>& task, const std::chrono::duration<uint32_t, std::milli>& timeout);

private:
  std::vector<std::thread> work_threads_;
  std::atomic<bool> exiting_ { false };
  std::atomic<size_t> in_flight_task_count_ { 0 };

  std::mutex pending_queue_mu_;
  std::condition_variable pending_queue_cv_;
  std::deque<std::unique_ptr<Task>> pending_queue_;

  std::mutex completed_queue_mu_;
  std::condition_variable completed_queue_cv_;
  std::deque<std::unique_ptr<Task>> completed_queue_;
};

} // namespace common
