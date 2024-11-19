// Copyright 2023, David Goodwin. All rights reserved.

#include <cerrno>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <sys/time.h>
#include "common_error.h"
#include "common_logger.h"
#include "common_util.h"

namespace common {

StreamLogger::StreamLogger(
  Logger::Level filter_level, const Timestamp timestamp_kind, std::ostream& of) :
    Logger(filter_level, timestamp_kind), out_stream_(of)
{
}

StreamLogger::StreamLogger(Logger::Level filter_level, const Timestamp timestamp_kind,
                           const std::string& filename, bool buffered) :
  Logger(filter_level, timestamp_kind), out_stream_(ofs_)
{
  if (!buffered) {
    ofs_.rdbuf()->pubsetbuf(0, 0);
  }
  ofs_.open(filename);
  THROWIF(ofs_.fail(), "unable to open '" + filename + "': " +  std::strerror(errno));
}

StreamLogger::StreamLogger(const Logger& other, std::ostream& of) :
  Logger(other), out_stream_(of)
{
}

StreamLogger::StreamLogger(const Logger& other, const std::string& filename, bool buffered) :
  Logger(other), out_stream_(ofs_)
{
  if (!buffered) {
    ofs_.rdbuf()->pubsetbuf(0, 0);
  }
  ofs_.open(filename);
  THROWIF(ofs_.fail(), "unable to open '" + filename + "': " +  std::strerror(errno));
}

void
StreamLogger::LogTimestamp()
{
  switch (timestamp_kind_) {
  case Timestamp::NONE:
    break;
  case Timestamp::COMPACT: {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm tm_time;
    localtime_r(((time_t*)&(tv.tv_sec)), &tm_time);
    out_stream_
      << "[" << std::setw(4) << (tm_time.tm_year + 1900)
      << std::setfill('0')
      << std::setw(2) << (tm_time.tm_mon + 1)
      << std::setw(2) << tm_time.tm_mday << ' '
      << std::setw(2) << tm_time.tm_hour << ':'
      << std::setw(2) << tm_time.tm_min << ':'
      << std::setw(2) << tm_time.tm_sec << '.'
      << std::setw(6) << tv.tv_usec << "] ";
  } break;
  }
}

void
StreamLogger::Log(const std::string& msg, size_t indent, bool endline)
{
  static std::mutex mu;
  std::lock_guard<std::mutex> lk(mu);

  // Timestamp is never indented and for a multiline 'msg' goes on a
  // line by itself.
  if (timestamp_kind_ != Timestamp::NONE) {
    LogTimestamp();
    if (msg.find('\n') != std::string::npos) {
      out_stream_ << '\n';
    }
  }
  if (indent == 0) {
    out_stream_ << msg;
  }
  else {
    std::string imsg(msg);
    common::Indent(imsg, 2);
    out_stream_ << imsg;
  }

  if (endline) {
    out_stream_ << std::endl;
  }
}

} // namespace common
