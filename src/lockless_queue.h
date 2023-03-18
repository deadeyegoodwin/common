// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

extern "C" {
#include "ck_fifo.h"
}

namespace common {

//
// LocklessMultiQueue
//
// A lockless, multi-producer / multi-consumer queue. A queue object
// must be wrapped in a std::unique_ptr for enqueuing/dequeuing so
// that ownership is clearly passed to/from the queue (and because the
// underlying implementation only supports pointer payload).  This
// class wraps concurrencykit's FIFO-mpmc implementation, see
// concurrencykit.org.
//
// Operations that add/remove entries to the queue need a
// LocklessMultiQueueAllocator object which is responsible for
// creating and managing internally used ck_fifo_mpmc_entry_t
// objects. A couple of allocators are provided but others can be
// derived. For LocklessMultiQueue destruction, free() is used to
// deallocate any entries remaining in the queue. Therefore, any
// LocklessMultiQueueAllocator implementation must allocate entries in
// a way that is compatible with those entries being deleted by
// free().
//

//
// LocklessMultiQueueAllocator
//
// Base implementation uses malloc()/free() for entry allocation.
//
class LocklessMultiQueueAllocator {
public:
  virtual ~LocklessMultiQueueAllocator() = default;
  virtual ck_fifo_mpmc_entry_t* Allocate() {
    return (ck_fifo_mpmc_entry_t*)malloc(sizeof(ck_fifo_mpmc_entry_t));
  }
  virtual void Release(ck_fifo_mpmc_entry_t* entry) {
    free(entry);
  }
  virtual void Clear() { }
};

//
// LocklessMultiQueuePoolAllocator
//
// Allocator class that maintains a pool of allocations for
// reuse/recycle. No locking is used so the caller must manage the
// thread-safety of using this allocator. Typically this is done by
// ensuring that each thread uses a unique
// LocklessMultiQueuePoolAllocator.
//
class LocklessMultiQueuePoolAllocator : public LocklessMultiQueueAllocator {
public:
  LocklessMultiQueuePoolAllocator(size_t pool_max_size = 1024) :
    pool_max_size_(pool_max_size), pool_size_(0), pool_(nullptr) { }
  ~LocklessMultiQueuePoolAllocator() { Clear(); }

  virtual ck_fifo_mpmc_entry_t* Allocate() override {
    ck_fifo_mpmc_entry_t* entry;
    if (pool_ != nullptr) {
      entry = pool_;
      pool_ = (ck_fifo_mpmc_entry_t*)pool_->value;
      --pool_size_;
    }
    else {
      entry = (ck_fifo_mpmc_entry_t*)malloc(sizeof(ck_fifo_mpmc_entry_t));
    }
    return entry;
  }

  virtual void Release(ck_fifo_mpmc_entry_t* entry) override {
    if (entry != nullptr) {
      if (pool_size_ >= pool_max_size_) {
        free(entry);
      }
      else {
        entry->value = pool_;
        pool_ = entry;
        ++pool_size_;
      }
    }
  }

  virtual void Clear() override {
    while (pool_ != nullptr) {
      ck_fifo_mpmc_entry_t* entry = pool_;
      pool_ = (ck_fifo_mpmc_entry_t*)pool_->value;
      free(entry);
    }
    pool_size_ = 0;
  }

private:
  const size_t pool_max_size_;
  size_t pool_size_;
  ck_fifo_mpmc_entry_t* pool_;
};

//
// LocklessMultiQueue
//
template <typename T>
class LocklessMultiQueue {
public:
  LocklessMultiQueue() {
    ck_fifo_mpmc_entry_t* init_stub =
      (ck_fifo_mpmc_entry_t*)malloc(sizeof(ck_fifo_mpmc_entry_t));
    ck_fifo_mpmc_init(&fifo_, init_stub);
  }

  // Destroy the queue. All enqueued T objects are destroyed.
  ~LocklessMultiQueue() {
    LocklessMultiQueueAllocator allocator;
    std::unique_ptr<T> t;
    while (Dequeue(t, allocator)) {
      t.reset();
    }

    ck_fifo_mpmc_entry_t* garbage;
    ck_fifo_mpmc_deinit(&fifo_, &garbage);
    free(garbage);
  }

  // Return true if the queue is empty.
  bool IsEmpty() { return CK_FIFO_MPMC_ISEMPTY(&fifo_); }

  // Enqueue a T object into the queue. Does not return until the
  // object is enqueued.
  void Enqueue(std::unique_ptr<T> t, LocklessMultiQueueAllocator& allocator) {
    ck_fifo_mpmc_entry_t* entry = allocator.Allocate();
    ck_fifo_mpmc_enqueue(&fifo_, entry, t.release());
  }

  // Try to enqueue a T object into the queue. Returns immediately:
  // true if the object was enqueued, false if not enqueued (in which
  // case 't' will still hold ownership of the object).
  bool TryEnqueue(std::unique_ptr<T>& t, LocklessMultiQueueAllocator& allocator) {
    ck_fifo_mpmc_entry_t* entry = allocator.Allocate();
    const bool res = ck_fifo_mpmc_tryenqueue(&fifo_, entry, t.get());
    if (res) {
      t.release();
    }
    else {
      allocator.Release(entry);
    }
    return res;
  }

  // Dequeue a T object from the queue. Returns immediately: false if
  // the queue is empty, true if object was dequeued (in which case 't'
  // will hold ownership of the dequeued object).
  bool Dequeue(std::unique_ptr<T>& t, LocklessMultiQueueAllocator& allocator) {
    void* value;
    ck_fifo_mpmc_entry_t* garbage = nullptr;
    const bool res = ck_fifo_mpmc_dequeue(&fifo_, &value, &garbage);
    if (res) {
      t.reset(reinterpret_cast<T*>(value));
    }
    allocator.Release(garbage);
    return res;
  }

private:
  ck_fifo_mpmc_t fifo_ CK_CC_CACHELINE;
};

} // namespace common
