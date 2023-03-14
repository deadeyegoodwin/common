// Copyright 2023, David Goodwin. All rights reserved.

#include "task.h"

namespace common {

thread_local ck_fifo_mpmc_entry_t* TaskLocklessMultiQueue::entry_pool_ = nullptr;

//
//
TaskPool::TaskPool(const size_t worker_count)
{
  if (worker_count == 0) {
    throw Exception("TaskPool requires at least 1 worker");
  }

  // The "work loop" for each thread. Waits for new task to be
  // available, executes the task and then places the task on the
  // completed queue.
  const auto work_loop = [this]() {
    while (true) {
      std::unique_ptr<Task> task;

      // Get pending task...
      {
        std::unique_lock<std::mutex> lk(pending_queue_mu_);
        pending_queue_cv_.wait(lk, [&]() -> bool { return !pending_queue_.empty() || exiting_; });
        if (exiting_ && pending_queue_.empty()) {
          break;
        }

        task = std::move(pending_queue_.front());
        pending_queue_.pop_front();
      }

      // Invoke the task. If TaskInvoke takes ownership of 'task' then
      // don't return it on the completed queue.
      Task* rtask = task.release();
      if (!rtask->Invoke(0)) {
        task.reset(rtask);

        std::lock_guard<std::mutex> lk(completed_queue_mu_);
        completed_queue_.push_back(std::move(task));
        completed_queue_cv_.notify_one();
      }
    }
  };

  work_threads_.reserve(worker_count);
  for (size_t i = 0; i < worker_count; ++i) {
    work_threads_.emplace_back(work_loop);
  }
}

TaskPool::~TaskPool()
{
  exiting_ = true;
  pending_queue_cv_.notify_all();
  for (auto& thd : work_threads_) {
    if (thd.joinable()) {
      thd.join();
    }
  }

  completed_queue_cv_.notify_all();
}

size_t
TaskPool::InFlightTaskCount() const
{
  return in_flight_task_count_;
}

void
TaskPool::Enqueue(std::unique_ptr<Task> task)
{
  in_flight_task_count_ += 1;

  std::lock_guard<std::mutex> lk(pending_queue_mu_);
  pending_queue_.push_back(std::move(task));
  pending_queue_cv_.notify_one();
}

bool
TaskPool::Dequeue(std::unique_ptr<Task>& task)
{
  std::unique_lock<std::mutex> lk(completed_queue_mu_);
  completed_queue_cv_.wait(lk, [&]() -> bool { return !completed_queue_.empty() || exiting_; });
  if (exiting_ && completed_queue_.empty()) {
    return false;
  }

  task = std::move(completed_queue_.front());
  completed_queue_.pop_front();

  in_flight_task_count_ -= 1;

  return true;
}

bool
TaskPool::Dequeue(std::unique_ptr<Task>& task,
                  const std::chrono::duration<uint32_t, std::milli>& timeout)
{
  std::unique_lock<std::mutex> lk(completed_queue_mu_);
  if (!completed_queue_cv_.wait_for(
        lk, timeout, [&]() -> bool { return !completed_queue_.empty() || exiting_; })) {
    return false;
  }
  if (exiting_ && completed_queue_.empty()) {
    return false;
  }

  task = std::move(completed_queue_.front());
  completed_queue_.pop_front();

  in_flight_task_count_ -= 1;

  return true;
}

//
// TaskTimewheelWorkerExitException
//
// Custom exception to allow workers to exit from with Task::Invoke.
//
class TaskTimewheelWorkerExitException : public std::exception {};

//
// TimewheelControlTask
//
// Task responsible handling changes in the control state of a
// TaskTimewheelPool.
//
class TimewheelControlTask : public common::Task {
public:
  enum class Kind { PAUSE_MASTER, PAUSE, UNPAUSE, STOP_MASTER, STOP };

  TimewheelControlTask(Kind kind, TaskTimewheelPool& pool) :
    kind_(kind), pool_(pool) { }

  bool Invoke(uint64_t time_step) override {
    switch (kind_) {
      // Enqueue one PAUSE task for each worker thread in the next
      // time-step. We need to do this enqueue from a worker thread to
      // ensure that all threads are active in the time-step when they
      // see the PAUSE (if we don't do it this way there could be some
      // threads in the "next" barrier and others that
      // pause... resulting in a hang).
    case Kind::PAUSE_MASTER:
      pool_.worker_pause_cnt_ = pool_.worker_count_;
      for (size_t w = 0; w < pool_.worker_count_; ++w) {
        pool_.Enqueue(std::make_unique<TimewheelControlTask>(
                        TimewheelControlTask::Kind::PAUSE, pool_), 1 /* time_delta */);
      }
      break;

      // Pause this thread if in PAUSING state. Once all worker
      // threads are paused, change to PAUSED state. This is the only
      // place where PAUSING->PAUSE, and from the PAUSING state the
      // only other possible transition is to STOPPING.
    case Kind::PAUSE: {
      std::unique_lock<std::mutex> control_lk(pool_.control_mu_);
      if (pool_.control_state_ == TaskTimewheelPool::ControlState::PAUSING) {
        if (--pool_.worker_pause_cnt_ == 0) {
          pool_.control_state_ = TaskTimewheelPool::ControlState::PAUSED;
          pool_.control_cv_.notify_all();
        }
        pool_.control_cv_.wait(control_lk, [this]() -> bool {
          return ((pool_.control_state_ != TaskTimewheelPool::ControlState::PAUSING) &&
                  (pool_.control_state_ != TaskTimewheelPool::ControlState::PAUSED)); });
      }
      else if (pool_.control_state_ != TaskTimewheelPool::ControlState::STOPPING) {
        throw common::Exception(
          "TaskTimewheelPool worker encountered unexpected PAUSE state: " +
          TaskTimewheelPool::ControlStateToName(pool_.control_state_));
      }
    } break;

      // Unpause this thread if in UNPAUSING state. Once all worker
      // threads are unpaused, change to RUNNING state. This is the
      // only place where UNPAUSING->RUNNING, and from the UNPAUSING
      // state the only other possible transition is to STOPPING.
    case Kind::UNPAUSE: {
      std::unique_lock<std::mutex> control_lk(pool_.control_mu_);
      if (pool_.control_state_ == TaskTimewheelPool::ControlState::UNPAUSING) {
        if (--pool_.worker_unpause_cnt_ == 0) {
          pool_.control_state_ = TaskTimewheelPool::ControlState::RUNNING;
          pool_.control_cv_.notify_all();
        } else {
          pool_.control_cv_.wait(control_lk, [this]() -> bool {
            return pool_.control_state_ != TaskTimewheelPool::ControlState::UNPAUSING; });
        }
      }
      else if (pool_.control_state_ != TaskTimewheelPool::ControlState::STOPPING) {
        throw common::Exception(
          "TaskTimewheelPool worker encountered unexpected UNPAUSE state: " +
          TaskTimewheelPool::ControlStateToName(pool_.control_state_));
      }
    } break;

      // Enqueue one STOP task for each worker thread in the next
      // time-step. We need to do this enqueue from a worker thread to
      // ensure that all threads are active in the time-step when they
      // see the STOP (if we don't do it this way there could be some
      // threads stuck in the "next" barrier).
    case Kind::STOP_MASTER:
      pool_.worker_stop_cnt_ = pool_.worker_count_;
      for (size_t w = 0; w < pool_.worker_count_; ++w) {
        pool_.Enqueue(std::make_unique<TimewheelControlTask>(
                        TimewheelControlTask::Kind::STOP, pool_), 1 /* time_delta */);
      }
      break;

      // Exit this thread. The last work worker thread to exit changes
      // state to STOPPED. This is the only place where
      // STOPPING->STOPPED.
    case Kind::STOP: {
      std::unique_lock<std::mutex> control_lk(pool_.control_mu_);
      if (pool_.control_state_ != TaskTimewheelPool::ControlState::STOPPING) {
        throw common::Exception("TaskTimewheelPool worker expected STOPPING state");
      }
      if (--pool_.worker_stop_cnt_ == 0) {
        pool_.control_state_ = TaskTimewheelPool::ControlState::STOPPED;
        pool_.control_cv_.notify_all();
      }
      throw TaskTimewheelWorkerExitException();
    } break;
    }

    return false;
  }

private:
  Kind kind_;
  TaskTimewheelPool& pool_;
};

//
//
TaskTimewheelPool::TaskTimewheelPool(const size_t worker_count, const size_t bucket_count) :
  control_state_(ControlState::RUNNING), worker_count_(worker_count),
  bucket_count_(bucket_count), time_buckets_(bucket_count),
  next_barrier_(CK_BARRIER_CENTRALIZED_INITIALIZER), in_flight_task_count_(0)
{
  if (worker_count_ == 0) {
    throw Exception("TaskTimewheelPool requires at least 1 worker");
  }
  if (bucket_count_ <= 0) {
    throw Exception("TaskTimewheelPool requires at least 1 time bucket");
  }

  // Pause the pool at the start... Directly enqueue a PAUSE task for
  // each worker so that the first task executed by each worker is a
  // PAUSE.
  control_state_ = ControlState::PAUSING;
  worker_pause_cnt_ = worker_count_;
  for (size_t w = 0; w < worker_count_; ++w) {
    Enqueue(std::make_unique<TimewheelControlTask>(
              TimewheelControlTask::Kind::PAUSE, *this), 0 /* time_delta */);
  }

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

    switch (control_state_) {
    case ControlState::STOPPED:
      return true;

      // If not already STOPPING, enter STOPPING state and fall-through
      // to wait for completion of stop.
    case ControlState::DRAINING:
    case ControlState::PAUSING:
    case ControlState::UNPAUSING:
    case ControlState::PAUSED:
    case ControlState::RUNNING:
      control_state_ = ControlState::STOPPING;
      Enqueue(std::make_unique<TimewheelControlTask>(
                TimewheelControlTask::Kind::STOP_MASTER, *this), 0);
      control_cv_.notify_all();
      // fall-through

      // If already stopping, wait for completion.
    case ControlState::STOPPING:
      control_cv_.wait(control_lk, [this]() -> bool {
        return control_state_ != ControlState::STOPPING; });
      break;
    }

    if (control_state_ != ControlState::STOPPED) {
      throw common::Exception("expected STOPPED state in TaskTimewheelPool::Stop(), "
                              "got " + ControlStateToName(control_state_));
    }
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
         (control_state_ == ControlState::PAUSING) ||
         (control_state_ == ControlState::UNPAUSING)) {
    if (control_state_ == ControlState::PAUSED) {
      control_lk.unlock();
      if (!Run()) {
        return false;
      }
      control_lk.lock();
    } else {
      control_cv_.wait(control_lk, [this]() -> bool {
        return((control_state_ != ControlState::PAUSING) &&
               (control_state_ != ControlState::UNPAUSING)); });
    }
  }

  switch (control_state_) {
  case ControlState::STOPPING:
  case ControlState::STOPPED:
    return false;

  case ControlState::PAUSED:
  case ControlState::PAUSING:
  case ControlState::UNPAUSING:
    throw common::Exception(
      "unexpected PAUSED/PAUSING/UNPAUSING state in TaskTimewheelPool::Drain()");
    return false;

    // If running, enter DRAINING state and fall-through to wait for
    // completion of the drain.
  case ControlState::RUNNING:
    control_state_ = ControlState::DRAINING;
    control_cv_.notify_all();
    // fall-through

    // If already draining, wait for drain completion.
  case ControlState::DRAINING:
    control_cv_.wait(control_lk, [this]() -> bool {
      return control_state_ != ControlState::DRAINING; });
    break;
  }

  return true;
}

bool
TaskTimewheelPool::Pause(bool block)
{
  std::unique_lock<std::mutex> control_lk(control_mu_);

  // Pausing and unpausing are transient states that we must wait to
  // complete before attempting to transition to pause state.
  control_cv_.wait(control_lk, [this]() -> bool {
    return((control_state_ != ControlState::PAUSING) &&
           (control_state_ != ControlState::UNPAUSING)); });

  switch (control_state_) {
  case ControlState::STOPPING:
  case ControlState::STOPPED:
    return false;

  case ControlState::PAUSED:
    return true;

  case ControlState::PAUSING:
  case ControlState::UNPAUSING:
    throw common::Exception(
      "unexpected PAUSING/UNPAUSING state in TaskTimewheelPool::Pause()");
    return false;

    // If not already PAUSING, enter PAUSING state and wait for
    // completion of pause (if blocking).
  case ControlState::DRAINING:
  case ControlState::RUNNING:
    control_state_ = ControlState::PAUSING;
    Enqueue(std::make_unique<TimewheelControlTask>(
              TimewheelControlTask::Kind::PAUSE_MASTER, *this), 0);
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

  // Pausing and unpausing are transient states that we must wait to
  // complete before attempting to transition to running state.
  control_cv_.wait(control_lk, [this]() -> bool {
    return((control_state_ != ControlState::PAUSING) &&
           (control_state_ != ControlState::UNPAUSING)); });

  switch (control_state_) {
  case ControlState::STOPPING:
  case ControlState::STOPPED:
    return false;

  case ControlState::PAUSING:
  case ControlState::UNPAUSING:
    throw common::Exception(
      "unexpected PAUSING/UNPAUSING state in TaskTimewheelPool::Run()");
    return false;

  case ControlState::RUNNING:
    return true;

    // If draining enter RUNNING state.
  case ControlState::DRAINING:
    control_state_ = ControlState::RUNNING;
    control_cv_.notify_all();
    break;

    // If PAUSED, enter UNPAUSING state and wait for completion. To
    // enter UNPAUSE state we don't need to use a "master"
    // TimewheelControlTask::Kind because we are in the PAUSED state
    // and so all pool threads are known to be at the same time-step.
  case ControlState::PAUSED:
    control_state_ = ControlState::UNPAUSING;
    worker_unpause_cnt_ = worker_count_;
    for (size_t w = 0; w < worker_count_; ++w) {
      Enqueue(std::make_unique<TimewheelControlTask>(
                TimewheelControlTask::Kind::UNPAUSE, *this), 0 /* time_delta */);
    }
    control_cv_.notify_all();
    control_cv_.wait(control_lk, [this]() -> bool {
      return control_state_ != ControlState::UNPAUSING; });
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
  case ControlState::UNPAUSING: {
    static const std::string pausing("UNPAUSING");
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
TaskTimewheelPool::Enqueue(std::unique_ptr<Task> task, uint32_t time_delta)
{
  ck_pr_inc_32(&in_flight_task_count_);
  task->payload_ = time_delta;
  input_queue_.Enqueue(std::move(task));
}

void
TaskTimewheelPool::WorkerThread()
{
  // Increments for every time-step.
  uint64_t generation_idx = 0;

  ck_barrier_centralized_state_t next_barrier_state = CK_BARRIER_CENTRALIZED_STATE_INITIALIZER;
  size_t bucket_idx = 0;

  // The "work loop" for each thread. Thread first tries queue for the
  // current bucket, if that is empty it then process input_queue_ to
  // place any incoming tasks into the appropriate bucket queue. If
  // both current bucket and input queue are empty then waits at the
  // "next barrier" for all other threads and then together they move
  // to the next bucket.
  while (true) {
    // Current bucket...
    std::unique_ptr<Task> task;
    if (time_buckets_[bucket_idx].Dequeue(task)) {
      // If there is a residue time-delta for the task, then
      // re-enqueue the task in the appropriate future bucket.
      size_t time_delta = task->payload_;
      if (time_delta > 0) {
        if (time_delta >= bucket_count_) {
          time_delta = (bucket_count_ - 1);
          task->payload_ -= time_delta;
        }
        else {
          task->payload_ = 0;
        }

        const size_t idx = (bucket_idx + time_delta) % bucket_count_;
        time_buckets_[idx].Enqueue(std::move(task));
      }
      // ... otherwise invoke the task.
      else {
        bool exit_worker = false;
        bool rtask_owner = false;
        Task* rtask = task.release();
        try {
          rtask_owner = !rtask->Invoke(generation_idx);
        }
        catch (const TaskTimewheelWorkerExitException&) {
          exit_worker = true;
          rtask_owner = true;
        }

        if (rtask_owner) {
          delete rtask;
        }

        ck_pr_dec_32(&in_flight_task_count_);

        if (exit_worker) {
          break;
        }
      }
    }
    // Input queue...
    else if (input_queue_.Dequeue(task)) {
      // 'payload_' is the time-delta for the task, use it to
      // determine the appropriate bucket. If time-delta is farther in
      // the future than the most-future bucket, then enqueue it in
      // the most-future bucket with 'payload_' set to the remaining
      // time-delta. When it is removed from the most-future bucket it
      // will be reevaluated for the correct bucket.
      size_t time_delta = task->payload_;
      if (time_delta >= bucket_count_) {
        time_delta = (bucket_count_ - 1);
        task->payload_ -= time_delta;
      }
      else {
        task->payload_ = 0;
      }

      const size_t idx = (bucket_idx + time_delta) % bucket_count_;
      time_buckets_[idx].Enqueue(std::move(task));
    }
    // Next barrier, synchronize threads so that they all receive any
    // pending control signal and all move on to the next bucket
    // together.
    else {
      // If there are no in-flight tasks then check to see if pool is
      // currently DRAINING. If it is then transition to the PAUSING
      // state.
      if (ck_pr_load_32(&in_flight_task_count_) == 0) {
        std::unique_lock<std::mutex> control_lk(control_mu_);
        if (control_state_ == ControlState::DRAINING) {
          control_state_ = ControlState::PAUSING;
          Enqueue(std::make_unique<TimewheelControlTask>(
                    TimewheelControlTask::Kind::PAUSE_MASTER, *this), 0);
          control_cv_.notify_all();
        }
      }

      ck_barrier_centralized(&next_barrier_, &next_barrier_state, worker_count_);

      generation_idx++;
      bucket_idx = (bucket_idx + 1) % bucket_count_;
    }
  }

  // Free all queue entries allocated in thread_local for this thread.
  ThreadFree();
}

} // namespace common
