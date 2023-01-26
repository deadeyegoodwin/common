// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <deque>
#include <memory>
#include <thread>
#include "error.h"

namespace common {

//
// TaskQueue
//
// A thread pool that executes enqueued tasks and then returns the
// completed tasks on another queue. A task object must be wrapped in
// a unique_ptr for enqueuing so that ownership is clearly passed to
// the task queue. The task must implement "void operator()()" which
// the task-queue will invoke to perform the task.
//
template <class T>
class TaskQueue {
public:
  TaskQueue(const size_t worker_cnt);
  ~TaskQueue();

  size_t InFlightTaskCount() const;
  void Enqueue(std::unique_ptr<T> task);
  bool Dequeue(std::unique_ptr<T>& task);
  bool Dequeue(std::unique_ptr<T>& task, const std::chrono::duration<uint32_t, std::milli>& timeout);

private:
  std::vector<std::thread> work_threads_;
  std::atomic<bool> exiting_ { false };
  std::atomic<size_t> in_flight_task_count_ { 0 };

  std::mutex pending_queue_mu_;
  std::condition_variable pending_queue_cv_;
  std::deque<std::unique_ptr<T>> pending_queue_;

  std::mutex completed_queue_mu_;
  std::condition_variable completed_queue_cv_;
  std::deque<std::unique_ptr<T>> completed_queue_;
};

template <class T>
TaskQueue<T>::TaskQueue(const size_t worker_cnt)
{
  if (worker_cnt == 0) {
    throw Exception("TaskQueue requires at least 1 worker");
  }

  // The "work loop" for each thread. Waits for new task to be
  // available, executes the task and then places the task on the
  // completed queue.
  const auto work_loop = [this]() {
    while (true) {
      std::unique_ptr<T> task;

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
        (*task)();
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

template <class T>
TaskQueue<T>::~TaskQueue()
{
  exiting_ = true;
  pending_queue_cv_.notify_all();
  for (auto& thd : work_threads_) {
    thd.join();
  }

  completed_queue_cv_.notify_all();
}

template <class T>
size_t
TaskQueue<T>::InFlightTaskCount() const
{
  return in_flight_task_count_;
}

template <class T>
void
TaskQueue<T>::Enqueue(std::unique_ptr<T> task)
{
  in_flight_task_count_ += 1;

  std::lock_guard<std::mutex> lk(pending_queue_mu_);
  pending_queue_.push_back(std::move(task));
  pending_queue_cv_.notify_one();
}

template <class T>
bool
TaskQueue<T>::Dequeue(std::unique_ptr<T>& task)
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

template <class T>
bool
TaskQueue<T>::Dequeue(std::unique_ptr<T>& task,
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
