// Copyright 2023, David Goodwin. All rights reserved.

#include "common_task_timewheel.h"

namespace common {

TaskTimewheelPool::TaskTimewheelPool(const size_t worker_count, const size_t bucket_count) :
  worker_count_(worker_count), pause_barrier_(worker_count), stop_barrier_(worker_count),
  bucket_count_(bucket_count), time_buckets_(bucket_count),
  in_flight_task_count_(0), next_time_step_barrier_(worker_count)

{
  THROWIF(worker_count_ == 0, "TaskTimewheelPool requires at least 1 worker");
  THROWIF(bucket_count_ <= 1, "TaskTimewheelPool requires at least 2 time buckets");

  // Pause the pool at the start...
  control_state_ = ControlState::PAUSING;
  control_state_needs_handle_ = false;
  next_time_step_handle_control_ = false;

  work_threads_.reserve(worker_count_);
  for (size_t i = 0; i < worker_count_; ++i) {
    work_threads_.emplace_back([this, i]() { WorkerThread(); });
  }
}

TaskTimewheelPool::~TaskTimewheelPool()
{
  Stop();
}

bool
TaskTimewheelPool::Stop()
{
  {
    std::unique_lock<std::mutex> control_lk(control_mu_);

    // Already stopped. All pool threads will/were joined by earlier
    // call to Stop() that transitioned us into STOPPED state, so can
    // just return.
    if (control_state_ == ControlState::STOPPED) {
      return true;
    }

    // If already stopping, wait for completion. The only possible
    // transition is STOPPING -> STOPPED. All pool threads will/were
    // joined by earlier call to Stop() that transitioned us into
    // STOPPED state, so can just return.
    if (control_state_ == ControlState::STOPPING) {
      control_cv_.wait(control_lk, [this]() -> bool {
        return control_state_ == ControlState::STOPPED; });
      return true;
    }

    // Can only enter STOPPING from PAUSED, so keep trying to pause
    // until successful.
    while (control_state_ != ControlState::PAUSED) {
      control_lk.unlock();
      if (!Pause()) {
        return false;
      }
      control_lk.lock();
    }

    switch (control_state_) {
    case ControlState::DRAINING:
    case ControlState::PAUSING:
    case ControlState::RUNNING:
    case ControlState::STOPPED:
    case ControlState::STOPPING:
      THROW("unexpected non-PAUSED state in TaskTimewheelPool::Stop()");
      return false;

      // If PAUSED, enter STOPPING state and wait for STOPPED.
    case ControlState::PAUSED:
      control_state_ = ControlState::STOPPING;
      control_state_needs_handle_ = true;
      control_cv_.notify_all();
      control_cv_.wait(control_lk, [this]() -> bool {
        return control_state_ != ControlState::STOPPING; });
      break;
    }

    THROWIF(control_state_ != ControlState::STOPPED,
            "expected STOPPED state in TaskTimewheelPool::Stop(), got " +
            ControlStateToName(control_state_));
  }

  for (auto& thd : work_threads_) {
    if (thd.joinable()) {
      thd.join();
    }
  }

  return true;
}

bool
TaskTimewheelPool::Drain()
{
  std::unique_lock<std::mutex> control_lk(control_mu_);

  // Before attempting to enter DRAINING state, we must first
  // transition out of any pause into the RUNNING state.
  while ((control_state_ == ControlState::PAUSED) ||
         (control_state_ == ControlState::PAUSING)) {
    if (control_state_ == ControlState::PAUSED) {
      control_lk.unlock();
      if (!Run()) {
        return false;
      }
      control_lk.lock();
    } else {
      control_cv_.wait(control_lk, [this]() -> bool {
        return(control_state_ != ControlState::PAUSING); });
    }
  }

  switch (control_state_) {
  case ControlState::STOPPING:
  case ControlState::STOPPED:
    return false;

  case ControlState::PAUSED:
  case ControlState::PAUSING:
    THROW("unexpected PAUSED/PAUSING state in TaskTimewheelPool::Drain()");
    return false;

    // If running, enter DRAINING state and fall-through to wait for
    // completion of the drain.
  case ControlState::RUNNING:
    control_state_ = ControlState::DRAINING;
    control_cv_.notify_all();
    // fall-through

    // If already draining, wait for drain (and the subsequent
    // pausing) to complete.
  case ControlState::DRAINING:
    control_cv_.wait(control_lk, [this]() -> bool {
      return((control_state_ != ControlState::DRAINING) &&
             (control_state_ != ControlState::PAUSING)); });
    break;
  }

  return true;
}

bool
TaskTimewheelPool::Pause(bool block)
{
  std::unique_lock<std::mutex> control_lk(control_mu_);

  switch (control_state_) {
  case ControlState::STOPPING:
  case ControlState::STOPPED:
    return false;

  case ControlState::PAUSED:
    break;

    // Already in PAUSING state, just wait for pause to complete
    // (unless not blocking).
  case ControlState::PAUSING:
    if (block) {
      control_cv_.wait(control_lk, [this]() -> bool {
        return control_state_ != ControlState::PAUSING; });
    }
    break;

    // If not already PAUSING, enter PAUSING state and wait for
    // completion of pause (if blocking).
  case ControlState::DRAINING:
  case ControlState::RUNNING:
    control_state_ = ControlState::PAUSING;
    control_state_needs_handle_ = true;
    control_cv_.notify_all();
    if (block) {
      control_cv_.wait(control_lk, [this]() -> bool {
        return control_state_ != ControlState::PAUSING; });
    }
    break;
  }

  return true;
}

bool
TaskTimewheelPool::Run()
{
  std::unique_lock<std::mutex> control_lk(control_mu_);

  // Pausing is a transient state that we must wait to complete before
  // attempting to transition to running state.
  control_cv_.wait(control_lk, [this]() -> bool {
    return control_state_ != ControlState::PAUSING; });

  switch (control_state_) {
  case ControlState::STOPPING:
  case ControlState::STOPPED:
    return false;

  case ControlState::PAUSING:
    THROW("unexpected PAUSING state in TaskTimewheelPool::Run()");
    return false;

  case ControlState::RUNNING:
    break;

  case ControlState::PAUSED:
  case ControlState::DRAINING:
    control_state_ = ControlState::RUNNING;
    control_cv_.notify_all();
    break;
  }

  return true;
}

const std::string&
TaskTimewheelPool::ControlStateToName(const ControlState state)
{
  switch (state) {
  case ControlState::STOPPING: {
    static const std::string stopping("STOPPING");
    return stopping;
  }
  case ControlState::STOPPED: {
    static const std::string stopped("STOPPED");
    return stopped;
  }
  case ControlState::DRAINING: {
    static const std::string draining("DRAINING");
    return draining;
  }
  case ControlState::PAUSING: {
    static const std::string pausing("PAUSING");
    return pausing;
  }
  case ControlState::PAUSED: {
    static const std::string paused("PAUSED");
    return paused;
  }
  case ControlState::RUNNING: {
    static const std::string running("RUNNING");
    return running;
  }
  }

  __builtin_unreachable();
}

void
TaskTimewheelPool::Enqueue(std::unique_ptr<Task>& task, uint32_t time_delta,
                           LocklessMultiQueueAllocator& allocator)
{
  ck_pr_inc_32(&in_flight_task_count_);
  task->remaining_time_steps_ = time_delta;
  input_queue_.Enqueue(std::move(task), allocator);
}

void
TaskTimewheelPool::Enqueue(std::unique_ptr<Task>&& task, uint32_t time_delta,
                           LocklessMultiQueueAllocator& allocator)
{
  ck_pr_inc_32(&in_flight_task_count_);
  task->remaining_time_steps_ = time_delta;
  input_queue_.Enqueue(std::move(task), allocator);
}

void
TaskTimewheelPool::WorkerThread()
{
  // Increments for every time-step.
  uint64_t generation_idx = 0;

  // Index into the time buckets, wraps...
  size_t bucket_idx = 0;

  // Use a lockless allocator for this thread. Lock is not needed
  // because this thread is the only user of this allocator.
  LocklessMultiQueuePoolAllocator entry_allocator;

  // Pause the pool at the start...
  if (!HandleControl()) {
    return;
  }

  // The "work loop" for each thread. Thread first tries queue for the
  // current bucket, if that is empty it then process input_queue_ to
  // place any incoming tasks into the appropriate bucket queue. If
  // both current bucket and input queue are empty then waits at the
  // "next barrier" for all other threads and then together they move
  // to the next bucket.
  while (true) {
    // Current bucket...
    std::unique_ptr<Task> task;
    if (time_buckets_[bucket_idx].Dequeue(task, entry_allocator)) {
      // If there is a residue time-delta for the task, then
      // re-enqueue the task in the appropriate future bucket.
      size_t time_delta = task->remaining_time_steps_;
      if (time_delta > 0) {
        if (time_delta >= bucket_count_) {
          time_delta = (bucket_count_ - 1);
          task->remaining_time_steps_ -= time_delta;
        }
        else {
          task->remaining_time_steps_ = 0;
        }

        const size_t idx = (bucket_idx + time_delta) % bucket_count_;
        time_buckets_[idx].Enqueue(std::move(task), entry_allocator);
      }
      // ... otherwise invoke the task.
      else {
        Task* rtask = task.release();
        if (!rtask->Invoke(generation_idx, entry_allocator)) {
          delete rtask;
        }

        ck_pr_dec_32(&in_flight_task_count_);
      }
    }
    // Input queue...
    else if (input_queue_.Dequeue(task, entry_allocator)) {
      // 'remaining_time_steps_' is the time-delta for the task, use
      // it to determine the appropriate bucket. If time-delta is
      // farther in the future than the most-future bucket, then
      // enqueue it in the most-future bucket with
      // 'remaining_time_steps_' set to the remaining time-delta. When
      // it is removed from the most-future bucket it will be
      // reevaluated for the correct bucket.
      size_t time_delta = task->remaining_time_steps_;
      if (time_delta >= bucket_count_) {
        time_delta = (bucket_count_ - 1);
        task->remaining_time_steps_ -= time_delta;
      }
      else {
        task->remaining_time_steps_ = 0;
      }

      const size_t idx = (bucket_idx + time_delta) % bucket_count_;
      time_buckets_[idx].Enqueue(std::move(task), entry_allocator);
    }
    // Next barrier, synchronize threads so that they all move on to
    // the next bucket together.
    else {
      // If the pool is draining and there are no in-flight tasks,
      // then transition to the PAUSING state.
      if (ck_pr_load_32(&in_flight_task_count_) == 0) {
        std::unique_lock<std::mutex> control_lk(control_mu_);
        if (control_state_ == ControlState::DRAINING) {
          control_state_ = ControlState::PAUSING;
          control_state_needs_handle_ = true;
          control_cv_.notify_all();
        }
      }

      // Just before the last thread releases the barrier, check to
      // see if there is any pending control command that needs to be
      // handled. If so set 'next_time_step_handle_control_' so that
      // all threads will see the pending control as they exit the
      // barrer.
      next_time_step_barrier_.Wait(
        nullptr /* pre */,
        [&](size_t count) { /* post */
          if ((count == 0) && control_state_needs_handle_) {
            control_state_needs_handle_ = false;
            next_time_step_handle_control_ = true;
          }});

      if (next_time_step_handle_control_) {
        if (!HandleControl()) {
          break;
        }
      }

      generation_idx++;
      bucket_idx = (bucket_idx + 1) % bucket_count_;
    }
  }
}

bool
TaskTimewheelPool::HandleControl()
{
  std::unique_lock<std::mutex> control_lk(control_mu_);

  // Pause this thread if in PAUSING state. Once all worker threads
  // are paused, change to PAUSED state. This is the only place where
  // PAUSING->PAUSED, and from the PAUSING state there is no other
  // possible transition.
  if (control_state_ == ControlState::PAUSING) {
    pause_barrier_.Wait(
      [&](size_t count) { control_lk.unlock(); }, /* pre */
      [&](size_t count) { /* post */
        control_lk.lock();
        if (count == 0) {
          next_time_step_handle_control_ = false;
          control_state_ = ControlState::PAUSED;
          control_cv_.notify_all();
        }});

    // Wait for non-PAUSED state. We are sure that all threads will
    // leave the below cv because once PAUSED state is exited the only
    // way to enter it again is by participation of all threads in the
    // above 'pause_barrier_'.
    control_cv_.wait(control_lk, [this]() -> bool {
      return control_state_ != ControlState::PAUSED; });

    // If pool was stopped while in the paused state it is very
    // important that we don't return and instead fall-through and
    // handle the STOPPING immediately. This will cause the workers to
    // then exit without doing any more work. If we instead return
    // here when in the stopping state, then the workers will do one
    // more time-step of work before stopping.
    //
    // We can only do this conditional return because we ensure that
    // any transition to STOPPING can only happend from PAUSED. For
    // example, we cannot transition from RUNNING to STOPPING. If we
    // did allow PAUSED -> RUNNING -> STOPPING, then when changing to
    // RUNNING, some (but not all) threads could release from the
    // above cv and then return, while others would not release until
    // after the change to STOPPING and so not return here and instead
    // wait at the below barrier for the other threads that will never
    // arrive (because they returned and will be waiting at
    // 'next_time_step_barrier_'.
    if (control_state_ != ControlState::STOPPING) {
      return true;
    }
  }

  // Stop this thread if in STOPPING state. This is the only place
  // where STOPPING->STOPPED, and from the STOPPING state there is no
  // other possible transition.
  if (control_state_ == ControlState::STOPPING) {
    stop_barrier_.Wait(
      [&](size_t count) { control_lk.unlock(); },
      [&](size_t count) {
        control_lk.lock();
        if (count == 0) {
          next_time_step_handle_control_ = false;
          control_state_ = ControlState::STOPPED;
          control_cv_.notify_all();
        }});

    return false;
  }

  THROW("TaskTimewheelPool worker encountered unexpected control state: " +
        ControlStateToName(control_state_));
}

} // namespace common
