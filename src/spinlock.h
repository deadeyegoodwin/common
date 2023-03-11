// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

extern "C" {
#include "ck_spinlock.h"
}

namespace common {

//
// Spinlock
//
// A fast spinlock. Not re-entrant by the same thread.
//
class Spinlock {
public:
  Spinlock() : spinlock_(CK_SPINLOCK_INITIALIZER) { }

  bool IsLocked() { return ck_spinlock_locked(&spinlock_); }
  void Lock() { ck_spinlock_lock(&spinlock_); }
  void Unlock() { ck_spinlock_unlock(&spinlock_); }

  // Try to acquire the lock. Returns true is lock was acquired, false
  // otherwise.
  bool TryLock() { return ck_spinlock_trylock(&spinlock_); }

  // Acquire the lock. Use exponential backoff while attempting to
  // acquire.
  void LockWithBackoff() { ck_spinlock_lock_eb(&spinlock_); }

private:
  Spinlock(const Spinlock& other) = delete;
  Spinlock(Spinlock&& other) = delete;
  Spinlock& operator=(const Spinlock& other) = delete;
  Spinlock& operator=(Spinlock&& other) = delete;

  ck_spinlock_t spinlock_ CK_CC_CACHELINE;
};

//
// ScopedSpinlock
//
// Scoped lock/unlock of a Spinlock.
//
class ScopedSpinlock {
public:
  ScopedSpinlock(Spinlock& spinlock) : spinlock_(spinlock) {
    spinlock_.Lock();
  }

  ~ScopedSpinlock() {
    spinlock_.Unlock();
  }

private:
  ScopedSpinlock(const ScopedSpinlock& other) = delete;
  ScopedSpinlock(ScopedSpinlock&& other) = delete;
  ScopedSpinlock& operator=(const ScopedSpinlock& other) = delete;
  ScopedSpinlock& operator=(ScopedSpinlock&& other) = delete;

  Spinlock& spinlock_;
};

} // namespace common
