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
#include "ck_fifo.h"
}

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
    ck_fifo_mpmc_entry_t* garbage;
    const bool res = ck_fifo_mpmc_dequeue(&fifo_, &value, &garbage);
    if (res) {
      task.reset(reinterpret_cast<Task*>(value));
    }
    ReleaseEntry(garbage);
    return res;
  }

  bool TryDequeue(std::unique_ptr<Task>& task) {
    void* value;
    ck_fifo_mpmc_entry_t* garbage;
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
  ck_fifo_mpmc_t fifo_;
};

} // namespace common
