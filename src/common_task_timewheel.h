// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "common_barrier.h"
#include "common_error.h"
#include "common_lockless_queue.h"

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
// TaskTimewheelPool::Task::Invoke() is called to execute each Task
// during the appropriate time-step. All Tasks within a timestep will
// have their Invoke() function called before any Tasks within a later
// timestep, but the order of Invoke() calls within a timestep can not
// be specified. If Invoke() returns true, indicating that the task
// has taken ownership of its own Task object, then TaskTimewheelPool
// does not perform any futher action on that task. If Invoke()
// returns false then TaskTimewheelPool retains ownership of the task,
// however, because TaskTimewheelPool has no futher use for the task,
// it will simply delete it.
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
    // invocation. Invoke() should return true to indicate that it is
    // retaining ownership of the task object, and return false to
    // indicate that it is returning ownership of the task object to
    // the pool. 'allocator' is the LocklessMultiQueue allocator to
    // use when calling Task::Enqueue() from within this Invoke()
    // exection.
    virtual bool Invoke(
      uint64_t time_step, LocklessMultiQueueAllocator& allocator) = 0;

    // Enqueue a task at relative 'time_delta'. Tasks can be enqueued
    // at the current time (time_delta == 0), but all pool threads may
    // not be available to process those tasks. Preferred usage is so
    // always enqueue tasks at a future time (time_delta >
    // 0). 'allocator' is the LocklessMultiQueue allocator to use when
    // enqueuing the task (use 'allocator' passed to Invoke()).
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

    // Used by pool to manage the scheduling of tasks farther in the
    // future than the number of time buckets.
    size_t remaining_time_steps_;
  };

public:
  enum class ControlState {
    RUNNING, DRAINING, PAUSING, PAUSED, STOPPING, STOPPED };
  static const std::string& ControlStateToName(const ControlState state);

  // Construct the pool. The pool starts in the PAUSED state.
  TaskTimewheelPool(const size_t worker_count_, const size_t time_bucket_count);

  // Destroy the pool. Any tasks remaining in the pool are dequeued
  // and destroyed.
  ~TaskTimewheelPool();

  // Return the current state of the pool.
  ControlState State() const { return control_state_; }

  // Stop the pool after the current time-step completes. If the pool
  // is currently paused, the pool will stop immeidately, before any
  // tasks are invoked. Even if the pool is not currently paused, no
  // tasks beyond those in the currently executing time-step will be
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
  // false if pool is in a state that does not allow draining.
  // Calling this function from Task::Invoke is unsupported and causes
  // undefined behavior.
  bool Drain();

  // Pause pool after it completes invocation of all tasks in the
  // currently executing time-step. If 'block' is true, function
  // returns after pool is paused or when another thread calls Run(),
  // Drain() or Stop(). If 'block' is false function returns
  // immediately. Calling Pause() does not prevent additional tasks
  // from being enqueued. Returns false if pool is in a state that
  // does not allowing pausing. Calling this function from
  // Task::Invoke is supported for 'block' == false, calling with
  // 'block' == true is unsupported and causes undefined behavior
  // (likely deadlock).
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
  void WorkerThread();

  // Handle control manages coordinating and updating the pool threads
  // in response to state changes initiated by Stop(), Drain(),
  // Pause() and Run(). The five functions work together to ensure
  // that only the following state transitions are allowed. Limiting
  // the state transitions in this way ensures that there is no
  // deadlock or other error.
  //
  // From RUNNING can enter the DRAINING (from Drain()) or PAUSING
  // (from Pause() or Stop()) states. Note that RUNNING cannot go
  // directly to STOPPING (see HandleControl()) for why this is not
  // allowed).
  //
  //   RUNNING -> DRAINING
  //           -> PAUSING
  //
  // From DRAINING can enter RUNNING (from Run()) or PAUSING (from
  // Pause()). As with RUNNING, DRAINING cannot go directly to
  // STOPPING (see HandleControl()) for why this is not allowed).
  //
  //   DRAINING -> RUNNING
  //            -> PAUSING
  //
  // From PAUSING the only transition is to PAUSED (managed in
  // HandleControl()). If Stop(), Drain() or Run() is invoked while in
  // PAUSING state, those methods will block until the transition to
  // PAUSED is complete.
  //
  //   PAUSING -> PAUSED
  //
  // From PAUSED can transition to RUNNING, DRAINING, or STOPPING. The
  // transition from PAUSED -> STOPPING is handled specially in
  // HandleControl() to ensure that no additional tasks are executed.
  //
  //   PAUSED -> RUNNING
  //          -> PAUSING
  //          -> STOPPING
  //
  // From STOPPING can only transition to STOPPED. The only transition
  // into STOPPING is from PAUSED and there are no transitions out of
  // STOPPED.
  //
  //   STOPPING -> STOPPED
  bool HandleControl();

  LocklessMultiQueueAllocator default_allocator_;

  std::mutex control_mu_;
  std::condition_variable control_cv_;
  ControlState control_state_;

  // Any thread (worker or non-worker) can set
  // 'control_state_needs_handle_' to true to cause the worker threads
  // to check and handle 'control_state_' after the next time-step
  // barrier. 'next_time_step_handle_control_' is used internally by
  // the worker threads to make sure that all worker threads handle
  // the control state together.
  std::atomic<bool> control_state_needs_handle_;
  std::atomic<bool> next_time_step_handle_control_;

  const size_t worker_count_;
  std::vector<std::thread> work_threads_;
  Barrier pause_barrier_;
  Barrier stop_barrier_;

  const size_t bucket_count_;
  std::vector<LocklessMultiQueue<Task>> time_buckets_;

  LocklessMultiQueue<Task> input_queue_ CK_CC_CACHELINE;
  uint32_t in_flight_task_count_ CK_CC_CACHELINE;

  Barrier next_time_step_barrier_; // FIXME lockless barrier
};

} // namespace common
