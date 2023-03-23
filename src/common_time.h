// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

#include <string>

namespace common {

//
// Time
//
// A time, could be a timestamp or duration.
//
class Time {
public:
  enum class TimeUnit {
    HOUR, MINUTE, SECOND, MILLISECOND, MICROSECOND
  };

  static TimeUnit TimeUnitFromName(const std::string& name);
  static const std::string& TimeUnitToName(const TimeUnit state);
  static size_t TimeUnitToMicroseconds(const TimeUnit time_unit);

public:
  // Create time from value and time unit.
  Time(size_t v = 0, TimeUnit unit = TimeUnit::MICROSECOND) :
    time_(v * TimeUnitToMicroseconds(unit)) { }

  // Create a Time object from a string. The string can be an integer
  // or real number but must have one of the valid time units (that
  // is, string representations of TimeUnit described in
  // TimeUnitToName()). Internally, time value is represented by a
  // single integer in the highest-precision unit (currently
  // MICROSECOND). If 'time_str' represents a value that is too small
  // to be represented in this precision then an exception is raised.
  Time(const std::string& time_str) : time_(TimeValueFromString(time_str)) { }

  Time& operator=(const Time& other) = default;
  Time& operator=(const std::string& time_str) {
    time_ = TimeValueFromString(time_str);
    return *this;
  }

  bool operator==(const Time& rhs) const { return time_ == rhs.time_; }
  bool operator!=(const Time& rhs) const { return time_ != rhs.time_; }
  bool operator<(const Time& rhs) const  { return time_ < rhs.time_; }
  bool operator>(const Time& rhs) const  { return time_ > rhs.time_; }
  bool operator<=(const Time& rhs) const { return time_ <= rhs.time_; }
  bool operator>=(const Time& rhs) const { return time_ >= rhs.time_; }

  Time& operator+=(const Time& rhs) { time_ += rhs.time_; return *this; }
  Time& operator-=(const Time& rhs) { time_ -= rhs.time_; return *this; }
  Time operator+(const Time& rhs) const { return Time(time_ + rhs.time_); }
  Time operator-(const Time& rhs) const { return Time(time_ - rhs.time_); }

  // Scale / multiple time by an integer.
  Time& operator*=(size_t s) { time_ *= s; return *this; }
  Time operator*(size_t s) const { return Time(time_ * s); }

  // Return the number of entire time 'delta's spanned by 'this' time.
  size_t NDeltas (const Time& delta) const {
    return time_ / delta.time_;
  }

  // Return the number of entire time 'delta's spanned by 'this'
  // time. Set 'this' time to equal the time not spanned by an entire
  // time 'delta' plus 'next'.
  size_t NDeltasNext (const Time& delta, const common::Time& next = Time()) {
    size_t w = time_ / delta.time_;
    time_ = (time_ % delta.time_) + next.time_;
    return w;
  }

  // Return the integral part of the time value in the given 'unit'.
  size_t Count(TimeUnit unit) const {
    return time_ / TimeUnitToMicroseconds(unit); }

  // Return string representation in 'unit' with exactly 'precision'
  // decimal places. The parameter defaults (MILLISECOND / 3) are
  // defined so that the full precision of any valid time will be
  // displayed.
  std::string ToString(Time::TimeUnit unit = Time::TimeUnit::MILLISECOND,
                       int precision = 3) const;

private:
  size_t TimeValueFromString(std::string time_str) const;

  // The time value, represented in the highest-precision unit
  // (currently MICROSECOND).
  size_t time_;
};

} // namespace common
