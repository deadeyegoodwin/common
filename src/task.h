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

extern "C" {
#include "ck_barrier.h"
#include "ck_fifo.h"
}

namespace common {

class TaskTimewheelPool;

//
// Task
//
// Base class for all task objects.
//
class Task {
public:
  virtual ~Task() = default;

  // Invoke task and transfer ownership of the task to the Invoke()
  // function. 'time_step' passes the time-step for the task
  // invocation, if that is meaningful. Invoke() should return true to
  // indicate that it is retaining ownership of the task object, and
  // return false to indicate that it is returning ownership of the
  // task object to the caller. See TaskPool, TaskTimewheelPool,
  // etc. for specific handling of this function.
  virtual bool Invoke(uint64_t time_step) = 0;

private:
  friend class TaskTimewheelPool;

  // Generic payload that can be used by TaskPool,
  // TaskLocklessMultiQueue, TaskTimewheelPool, etc.
  size_t payload_;
};

//
// TaskPool
//
// A thread pool that executes enqueued tasks and then returns the
// completed tasks on another queue (the "completed" queue). A task
// object must be wrapped in a std::unique_ptr for enqueuing/dequeuing
// so that ownership is clearly passed to/from the pool. The task must
// derive from class Task.
//
// Task::Invoke() is always called with time_step = 0. If
// Task::Invoke() returns false, indicating that TaskPool retains
// ownership of the task object, TaskPool returns that task on the
// completed queue. This is the normal behavior. If Task::Invoke()
// returns true, then TaskPool no longer has ownership of the task
// object and so will *not* return the task on the completed queue.
//
class TaskPool {
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

//
// TaskLocklessMultiQueue
//
// A lockless, multi-producer / multi-consumer queue.  A task object
// must be wrapped in a std::unique_ptr for enqueuing/dequeuing so
// that ownership is clearly passed to/from the task queue (and
// because the underlying implementation only supports pointer
// payload).  This class wraps concurrencykit's FIFO-mpmc
// implementation, see concurrencykit.org.
//
class TaskLocklessMultiQueue {
public:
  TaskLocklessMultiQueue() {
    ck_fifo_mpmc_entry_t* init_stub = NewEntry();
    ck_fifo_mpmc_init(&fifo_, init_stub);
  }

  ~TaskLocklessMultiQueue() {
    ck_fifo_mpmc_entry_t* garbage;
    ck_fifo_mpmc_deinit(&fifo_, &garbage);
    ReleaseEntry(garbage);
  }

  bool IsEmpty() { return CK_FIFO_MPMC_ISEMPTY(&fifo_); }

  void Enqueue(std::unique_ptr<Task> task) {
    ck_fifo_mpmc_entry_t* entry = NewEntry();
    ck_fifo_mpmc_enqueue(&fifo_, entry, task.release());
  }

  bool TryEnqueue(std::unique_ptr<Task> task) {
    ck_fifo_mpmc_entry_t* entry = NewEntry();
    const bool res = ck_fifo_mpmc_tryenqueue(&fifo_, entry, task.get());
    if (res) {
      task.release();
    }
    else {
      ReleaseEntry(entry);
    }
    return res;
  }

  bool Dequeue(std::unique_ptr<Task>& task) {
    void* value;
    ck_fifo_mpmc_entry_t* garbage = nullptr;
    const bool res = ck_fifo_mpmc_dequeue(&fifo_, &value, &garbage);
    if (res) {
      task.reset(reinterpret_cast<Task*>(value));
    }
    ReleaseEntry(garbage);
    return res;
  }

  bool TryDequeue(std::unique_ptr<Task>& task) {
    void* value;
    ck_fifo_mpmc_entry_t* garbage = nullptr;
    const bool res = ck_fifo_mpmc_trydequeue(&fifo_, &value, &garbage);
    if (res) {
      task.reset(reinterpret_cast<Task*>(value));
    }
    ReleaseEntry(garbage);
    return res;
  }

private:
  ck_fifo_mpmc_entry_t* NewEntry() {
    ck_fifo_mpmc_entry_t* entry;
    if (entry_pool_ != nullptr) {
      entry = entry_pool_;
      entry_pool_ = (ck_fifo_mpmc_entry_t*)entry_pool_->value;
    }
    else {
      entry = (ck_fifo_mpmc_entry_t*)malloc(sizeof(ck_fifo_mpmc_entry_t));
    }
    return entry;
  }

  void ReleaseEntry(ck_fifo_mpmc_entry_t* entry) {
    if (entry != nullptr) {
      entry->value = entry_pool_;
      entry_pool_ = entry;
    }
  }

  static thread_local ck_fifo_mpmc_entry_t* entry_pool_;
  ck_fifo_mpmc_t fifo_ CK_CC_CACHELINE;
};

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
// from class Task.
//
// Task::Invoke() is called with time_step = time N. If Task::Invoke()
// returns true, indicating that the task has taken ownership of its
// own Task object, then TaskTimewheelPool does not perform any futher
// action on that task. If Task::Invoke() returns false then
// TaskTimewheelPool retains ownership of the task, however, because
// TaskTimewheelPool has no futher use for the task, it will simply
// delete it.
//
class TaskTimewheelPool {
public:
  // Construct the pool. The pool starts in the PAUSED state.
  TaskTimewheelPool(const size_t worker_count_, const size_t time_bucket_count);
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
  // completes the tasks in the current time-step. Function returns
  // when pool is paused or when another thread calls Run() or
  // Stop(). Calling Pause() does not prevent additional tasks from
  // being enqueued. Returns false if pool is in a state that does not
  // allowing pausing. Calling this function from Task::Invoke is
  // unsupported and causes undefined behavior.
  bool Pause();

  // Run/unpause the pool. Function returns when pool has commenced
  // processing tasks. Returns false if pool is in a state that does
  // not allowing pausing. Calling this function from Task::Invoke is
  // unsupported and causes undefined behavior.
  bool Run();

  // Enqueue a task at relative 'time_delta'. Tasks can be enqueued at
  // the current time (time_delta == 0), but all pool threads may not
  // be available to process those tasks. Preferred usage it so always
  // enqueue tasks at a future time (time_delta > 0).
  void Enqueue(std::unique_ptr<Task> task, uint32_t time_delta);

private:
  friend class TimewheelControlTask;

  enum class ControlState { RUNNING, DRAINING, PAUSING, PAUSED, STOPPING, STOPPED };
  static const std::string& ControlStateToName(const ControlState state);

  void WorkerThread(const bool is_controller);

  std::mutex control_mu_;
  std::condition_variable control_cv_;
  ControlState control_state_;

  const size_t worker_count_;
  std::vector<std::thread> work_threads_;
  size_t worker_pause_cnt_;
  size_t worker_stop_cnt_;

  const size_t bucket_count_;
  std::vector<TaskLocklessMultiQueue> time_buckets_;

  TaskLocklessMultiQueue input_queue_ CK_CC_CACHELINE;
  ck_barrier_centralized_t next_barrier_ CK_CC_CACHELINE;
  uint32_t in_flight_task_count_ CK_CC_CACHELINE;
};

} // namespace common
