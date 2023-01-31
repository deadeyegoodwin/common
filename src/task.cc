// Copyright 2023, David Goodwin. All rights reserved.

#include "task.h"

namespace common {

TaskQueue::TaskQueue(const size_t worker_cnt)
{
  if (worker_cnt == 0) {
    throw Exception("TaskQueue requires at least 1 worker");
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

      // Execute...
      if (task != nullptr) {
        task->TaskInvoke();
      }

      // Return completed task...
      {
        std::lock_guard<std::mutex> lk(completed_queue_mu_);
        completed_queue_.push_back(std::move(task));
        completed_queue_cv_.notify_one();
      }
    }
  };

  work_threads_.reserve(worker_cnt);
  for (size_t i = 0; i < worker_cnt; ++i) {
    work_threads_.emplace_back(work_loop);
  }
}

TaskQueue::~TaskQueue()
{
  exiting_ = true;
  pending_queue_cv_.notify_all();
  for (auto& thd : work_threads_) {
    thd.join();
  }

  completed_queue_cv_.notify_all();
}

size_t
TaskQueue::InFlightTaskCount() const
{
  return in_flight_task_count_;
}

void
TaskQueue::Enqueue(std::unique_ptr<Task> task)
{
  in_flight_task_count_ += 1;

  std::lock_guard<std::mutex> lk(pending_queue_mu_);
  pending_queue_.push_back(std::move(task));
  pending_queue_cv_.notify_one();
}

bool
TaskQueue::Dequeue(std::unique_ptr<Task>& task)
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
TaskQueue::Dequeue(std::unique_ptr<Task>& task,
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

} // namespace common
