// Copyright 2023, David Goodwin. All rights reserved.

#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <utility>
#include "common_error.h"
#include "common_time.h"
#include "common_util.h"

namespace common {

namespace {

static const std::unordered_map<Time::TimeUnit, std::pair<std::string, size_t>> tunamemap = {
  { Time::TimeUnit::HOUR, { "h", (size_t)60 * 60 * 1000 * 1000 }},
  { Time::TimeUnit::MINUTE, { "m", (size_t)60 * 1000 * 1000 }},
  { Time::TimeUnit::SECOND, { "s", (size_t)1000 * 1000 }},
  { Time::TimeUnit::MILLISECOND, { "ms", (size_t)1000 }},
  { Time::TimeUnit::MICROSECOND, { "us", (size_t)1 }}
};

} // namespace

Time::TimeUnit
Time::TimeUnitFromName(const std::string& name)
{
  for (auto& pr : tunamemap) {
    if (pr.second.first == name) {
      return pr.first;
    }
  }

  THROW("unknown time unit '" + name + "'");
}

const std::string&
Time::TimeUnitToName(const TimeUnit time_unit)
{
  auto it = tunamemap.find(time_unit);
  THROWIF(it == tunamemap.end(), "internal");
  return it->second.first;
}

size_t
Time::TimeUnitToMicroseconds(const TimeUnit time_unit)
{
  auto it = tunamemap.find(time_unit);
  THROWIF(it == tunamemap.end(), "internal");
  return it->second.second;
}


size_t
Time::TimeValueFromString(std::string time_str) const
{
  Trim(time_str);

  THROWIF(time_str.find_first_of("-") != std::string::npos,
          "time value must be non-negative, '" + time_str + "'");

  // Time unit...
  const size_t upos = time_str.find_first_of("hmsu");
  THROWIF(upos == std::string::npos, "time value must have unit, '" + time_str + "'");

  // Get the unit, and the number of microseconds in each increment of
  // that unit of time.
  const Time::TimeUnit unit = TimeUnitFromName(time_str.substr(upos));
  const size_t us_per_unit = TimeUnitToMicroseconds(unit);

  // Whole number and fractional parts...
  std::string whole, fraction;
  const size_t dpos = time_str.find_first_of(".");
  if (dpos == std::string::npos) {
    whole = Trim(time_str.substr(0, upos));
  }
  else {
    whole = Trim(time_str.substr(0, dpos));
    fraction = Trim(time_str.substr(dpos + 1, upos - dpos - 1));
  }

  // Allow at most 6 fractional digits of precision. This simplifies
  // the following logic because TimeUnitToMicrosecond for all units
  // will be evenly divisible by 10^6.
  THROWIF(fraction.size() > 6,
          "time value can have at most 6 decimal digits of precision, '" + time_str + "'");

  size_t time_us = (whole.empty()) ? 0 : (std::stoull(whole) * us_per_unit);

  size_t us_per_digit = us_per_unit;
  for (auto& digit : fraction) {
    THROWIF(us_per_digit < 10,
        "time value cannot be represented, minimum precision 1us, '" + time_str + "'");

    us_per_digit /= 10;
    time_us += us_per_digit * size_t(digit - '0');
  }

  return time_us;
}

std::string
Time::ToString(Time::TimeFormat format, Time::TimeUnit unit, int precision) const
{
  std::stringstream ss;
  size_t stime = time_;

  switch (format) {
  case TimeFormat::FLAT:
    break;

  case TimeFormat::HMS: {
    const size_t h = stime / TimeUnitToMicroseconds(TimeUnit::HOUR);
    stime -= h * TimeUnitToMicroseconds(TimeUnit::HOUR);
    const size_t m = stime / TimeUnitToMicroseconds(TimeUnit::MINUTE);
    stime -= m * TimeUnitToMicroseconds(TimeUnit::MINUTE);
    ss << h << TimeUnitToName(TimeUnit::HOUR)
       << std::setw(2) << std::setfill('0') << m << TimeUnitToName(TimeUnit::MINUTE);
  } break;
  }

  const double t = stime / (double)TimeUnitToMicroseconds(unit);
  ss << std::fixed << std::setprecision(precision) << t << TimeUnitToName(unit);

  return ss.str();
}

} // namespace common
