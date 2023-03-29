// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <deque>
#include <memory>
#include <thread>
#include <vector>
#include "common_error.h"

namespace common {

//
// TaskPool
//
// A thread pool that executes enqueued tasks and then returns the
// completed tasks on another queue (the "completed" queue). A task
// object must be wrapped in a std::unique_ptr for enqueuing/dequeuing
// so that ownership is clearly passed to/from the pool. The task must
// derive from class TaskPool::Task.
//
// If Task::Invoke() returns false, indicating that TaskPool retains
// ownership of the task object, TaskPool returns that task on the
// completed queue. This is the normal behavior. If Task::Invoke()
// returns true, then TaskPool no longer has ownership of the task
// object and so will *not* return the task on the completed queue.
//
class TaskPool {
public:
  class Task {
  public:
    // A task that can be enqueued in the task-pool.
    virtual ~Task() = default;

    // Invoke task and transfer ownership of the task to the Invoke()
    // function. Invoke() should return true to indicate that it is
    // retaining ownership of the task object, and return false to
    // indicate that it is returning ownership of the task object to the
    // caller.
    virtual bool Invoke() = 0;
  };

public:
  TaskPool(const size_t worker_count);
  ~TaskPool();

  size_t InFlightTaskCount() const;
  void Enqueue(std::unique_ptr<Task> task);
  bool Dequeue(std::unique_ptr<Task>& task);
  bool Dequeue(std::unique_ptr<Task>& task,
               const std::chrono::duration<uint32_t, std::milli>& timeout);

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
