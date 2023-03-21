// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "barrier.h"
#include "error.h"
#include "lockless_queue.h"

extern "C" {
#include "ck_barrier.h"
}

namespace common {

//
// TaskTimewheelPool
//
// A thread pool that executes enqueued tasks. Tasks can be enqueued
// to execute immediately, or at a time in the future. Future time is
// indicated by an positive integer value indicating the time delta to
// when the task should execute. TaskTimewheelPool does not itself
// assign any actual time duration to a time delta value, it simply
// makes sure that all tasks enqueued for execution at time N are
// invoked (and Task::Invoke returns) before tasks at time N+1.
//
// A task object must be wrapped in a std::unique_ptr for enqueuing so
// that ownership is clearly passed to the pool. The task must derive
// from class TaskTimewheelPool::Task.
//
// TaskTimewheelPool::Task::Invoke() is called with time_step = time
// N. If Invoke() returns true, indicating that the task has taken
// ownership of its own Task object, then TaskTimewheelPool does not
// perform any futher action on that task. If Invoke() returns false
// then TaskTimewheelPool retains ownership of the task, however,
// because TaskTimewheelPool has no futher use for the task, it will
// simply delete it.
//
class TaskTimewheelPool {
public:
  // TaskTimewheelPool::Task. Base class for tasks enqueued and
  // invoked in timewheel-pool.
  class Task {
  public:
    // Create a task for 'pool'. 'pool' may be nullptr but then cannot
    // be used within Task::Invoke() nor can Task::Enqueue() be used.
    Task(TaskTimewheelPool* pool = nullptr) : pool_(pool) { }
    virtual ~Task() = default;

    // Invoke task and transfer ownership of the task to the Invoke()
    // function. 'time_step' passes the time-step for the task
    // invocation. Invoke() must return true to indicate that it is
    // retaining ownership of the task object, and return false to
    // indicate that it is returning ownership of the task object to
    // the caller. 'allocator' is the LocklessMultiQueue allocator to
    // use when calling Task::Enqueue() from within this Invoke()
    // exection.
    virtual bool Invoke(
      uint64_t time_step, LocklessMultiQueueAllocator& allocator) = 0;

    // Enqueue a task at relative 'time_delta'. Tasks can be enqueued at
    // the current time (time_delta == 0), but all pool threads may not
    // be available to process those tasks. Preferred usage it so always
    // enqueue tasks at a future time (time_delta > 0). 'allocator' is
    // the LocklessMultiQueue allocator to use when enqueuing the task.
    void Enqueue(std::unique_ptr<Task>& task, uint32_t time_delta,
                 LocklessMultiQueueAllocator& allocator) {
      pool_->Enqueue(task, time_delta, allocator);
    }
    void Enqueue(std::unique_ptr<Task>&& task, uint32_t time_delta,
                 LocklessMultiQueueAllocator& allocator) {
      pool_->Enqueue(task, time_delta, allocator);
    }

  protected:
    // The pool that is managing this task.
    TaskTimewheelPool* pool_;

  private:
    friend class TaskTimewheelPool;

    // Used to manage the scheduling of tasks farther in the future
    // than the number of time buckets.
    size_t remaining_time_steps_;
  };

public:
  // Construct the pool. The pool starts in the PAUSED state.
  TaskTimewheelPool(const size_t worker_count_, const size_t time_bucket_count);

  // Destroy the pool. Any tasks remaining in the pool are dequeued
  // and destroyed.
  ~TaskTimewheelPool();

  // Stop the pool so that any pending or future tasks will not be
  // invoked. Once stopped the pool cannot be restarted. Function
  // returns when pool is stopped. Returns false if pool is in a state
  // that does not allowing stopping. Calling this function from
  // Task::Invoke is unsupported and causes undefined behavior.
  bool Stop();

  // Drain all in-flight tasks from the pool. Function returns when
  // all in-flight tasks complete or when another thread calls
  // Pause(), Run() or Stop(). Calling Drain() does not prevent
  // additional tasks from being enqueued, and thus becoming
  // additional in-flight tasks that Drain() will wait on. Returns
  // false if pool is in a state that does not allowing draining.
  // Calling this function from Task::Invoke is unsupported and causes
  // undefined behavior.
  bool Drain();

  // Pause pool so that it does not process any tasks once it
  // completes the tasks in the current time-step. If 'block' is true,
  // function returns when pool is paused or when another thread calls
  // Run() or Stop(). If 'bloc' is false function returns
  // immediately. Calling Pause() does not prevent additional tasks
  // from being enqueued. Returns false if pool is in a state that
  // does not allowing pausing. Calling this function from
  // Task::Invoke is supported for 'block' == true, calling with
  // 'block' == false is unsupported and causes undefined behavior.
  bool Pause(bool block = true);

  // Run/unpause the pool. Function returns when all pool threads have
  // commenced processing tasks. Returns false if pool is in a state
  // that does not allowing running. Calling this function from
  // Task::Invoke is unsupported and causes undefined behavior.
  bool Run();

  // Enqueue a task at relative 'time_delta'. Tasks can be enqueued at
  // the current time (time_delta == 0), but all pool threads may not
  // be available to process those tasks. Preferred usage it so always
  // enqueue tasks at a future time (time_delta > 0). 'allocator' is
  // the LocklessMultiQueue allocator to use when enqueuing the
  // task. This method should only be used when enqueuing tasks from a
  // non-timewheel-pool thread. When enqueueing a task from a
  // Task::Invoke function should use the Task::Enqueue method.
  void Enqueue(std::unique_ptr<Task>& task, uint32_t time_delta,
               LocklessMultiQueueAllocator& allocator);
  void Enqueue(std::unique_ptr<Task>&& task, uint32_t time_delta,
               LocklessMultiQueueAllocator& allocator);

private:
  friend class TimewheelControlTask;

  enum class ControlState {
    RUNNING, DRAINING, PAUSING, PAUSED, STOPPING, STOPPED };
  static const std::string& ControlStateToName(const ControlState state);

  void WorkerThread();

  LocklessMultiQueueAllocator default_allocator_;

  std::mutex control_mu_;
  std::condition_variable control_cv_;
  ControlState control_state_;

  const size_t worker_count_;
  std::vector<std::thread> work_threads_;
  Barrier pause_barrier_;
  Barrier stop_barrier_;

  const size_t bucket_count_;
  std::vector<LocklessMultiQueue<Task>> time_buckets_;

  LocklessMultiQueue<Task> input_queue_ CK_CC_CACHELINE;
  ck_barrier_centralized_t next_barrier_ CK_CC_CACHELINE;
  uint32_t in_flight_task_count_ CK_CC_CACHELINE;
};

} // namespace common
