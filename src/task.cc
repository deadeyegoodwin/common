// Copyright 2023, David Goodwin. All rights reserved.

#include "task.h"

namespace common {

thread_local ck_fifo_mpmc_entry_t* TaskLocklessMultiQueue::entry_pool_ = nullptr;

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

TaskTimewheelPool::TaskTimewheelPool(const size_t worker_count, const size_t bucket_count) :
  control_request_(CONTROL_PAUSE), exit_generation_(0), worker_count_(worker_count),
  bucket_count_(bucket_count), time_buckets_(bucket_count),
  next_barrier_(CK_BARRIER_CENTRALIZED_INITIALIZER), in_flight_task_count_(0)
{
  if (worker_count_ == 0) {
    throw Exception("TaskTimewheelPool requires at least 1 worker");
  }
  if (bucket_count_ <= 0) {
    throw Exception("TaskTimewheelPool requires at least 1 time bucket");
  }

  work_threads_.reserve(worker_count_);
  for (size_t i = 0; i < worker_count_; ++i) {
    work_threads_.emplace_back([this, i]() { WorkerThread(i == 0); });
  }
}

TaskTimewheelPool::~TaskTimewheelPool()
{
  ck_pr_store_32(&control_request_, CONTROL_EXIT);
  drain_cv_.notify_all();
  pause_cv_.notify_all();
  for (auto& thd : work_threads_) {
    if (thd.joinable()) {
      thd.join();
    }
  }
}

void
TaskTimewheelPool::Drain()
{
  ck_pr_store_32(&control_request_, CONTROL_DRAIN);
  pause_cv_.notify_all();

  std::unique_lock<std::mutex> lk(drain_mu_);
  drain_cv_.wait(lk, [&]() -> bool { return ck_pr_load_32(&control_request_) != CONTROL_DRAIN; });
}

void
TaskTimewheelPool::Pause()
{
  ck_pr_store_32(&control_request_, CONTROL_PAUSE);
  drain_cv_.notify_all();
}

void
TaskTimewheelPool::Run()
{
  ck_pr_store_32(&control_request_, CONTROL_RUN);
  pause_cv_.notify_all();
  drain_cv_.notify_all();
}

void
TaskTimewheelPool::Enqueue(std::unique_ptr<Task> task, uint32_t time_delta)
{
  ck_pr_inc_32(&in_flight_task_count_);
  task->payload_ = time_delta;
  input_queue_.Enqueue(std::move(task));
}

void
TaskTimewheelPool::WorkerThread(const bool is_controller)
{
  // Increments for every time bucket. Must start at 1 (instead of 0)
  // as it is used with exit_generation_ to mark the generation where
  // threads should exit.
  uint64_t generation_idx = 1;

  ck_barrier_centralized_state_t next_barrier_state = CK_BARRIER_CENTRALIZED_STATE_INITIALIZER;
  size_t bucket_idx = 0;

  // If starting in PAUSED, then pause here...
  if (is_controller && (ck_pr_load_32(&control_request_) == CONTROL_PAUSE)) {
    const std::chrono::milliseconds timeout(100);
    std::unique_lock<std::mutex> lk(pause_mu_);
    pause_cv_.wait_for(lk, timeout, [&]() -> bool { return ck_pr_load_32(&control_request_) != CONTROL_PAUSE; });
  }

  ck_barrier_centralized(&next_barrier_, &next_barrier_state, worker_count_);

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
      else {
        Task* rtask = task.release();
        if (!rtask->Invoke(generation_idx)) {
          delete rtask;
        }
        ck_pr_dec_32(&in_flight_task_count_);
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
      // Controller thread processes any requested control action, and
      // signals an exit if necessary.
      if (is_controller) {
        uint32_t requested_control = ck_pr_load_32(&control_request_);
        // Requested DRAIN becomes PAUSE once all in-flight tasks
        // completed.
        if (requested_control == CONTROL_DRAIN) {
          if (ck_pr_load_32(&in_flight_task_count_) == 0) {
            requested_control = CONTROL_PAUSE;
            ck_pr_store_32(&control_request_, CONTROL_PAUSE);
            drain_cv_.notify_all();
          }
          else {
            requested_control = CONTROL_RUN;
          }
        }

        // Requested PAUSE causes the controlling thread to wait which
        // in turn causes all other threads to be blocked at barrier.
        if (requested_control == CONTROL_PAUSE) {
          const std::chrono::milliseconds timeout(100);
          std::unique_lock<std::mutex> lk(pause_mu_);
          pause_cv_.wait_for(lk, timeout, [&]() -> bool { return ck_pr_load_32(&control_request_) != CONTROL_PAUSE; });
          requested_control = CONTROL_RUN;
        }

        if (requested_control == CONTROL_EXIT) {
          ck_pr_store_64(&exit_generation_, generation_idx);
        }
      }

      ck_barrier_centralized(&next_barrier_, &next_barrier_state, worker_count_);

      if (ck_pr_load_64(&exit_generation_) == generation_idx) {
        break;
      }

      generation_idx++;
      bucket_idx = (bucket_idx + 1) % bucket_count_;
    }
  }
}

} // namespace common
