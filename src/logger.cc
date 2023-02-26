// Copyright 2023, David Goodwin. All rights reserved.

#include <cerrno>
#include <cstring>
#include <mutex>
#include "error.h"
#include "logger.h"

namespace common {

std::string
IndentMultiline(const std::string& str, const size_t indent)
{
  std::string mstr(indent, ' ');
  std::size_t pos = 0;
  std::size_t prev_pos;

  std::string rstr(indent, ' ');
  rstr.insert(rstr.begin(), '\n');

  mstr.reserve(str.size());
  while (true) {
    prev_pos = pos;
    pos = str.find('\n', pos);
    if (pos == std::string::npos)
      break;
    mstr.append(str, prev_pos, pos - prev_pos);
    mstr += rstr;
    pos += 1; // strlen(\n)
  }

  mstr.append(str, prev_pos, str.size() - prev_pos);
  return mstr;
}

StreamLogger::StreamLogger(Logger::Level level, std::ostream& of) :
  Logger(level), out_stream_(of)
{
}

StreamLogger::StreamLogger(Logger::Level level, const std::string& filename) :
  Logger(level), ofs_(filename), out_stream_(ofs_)
{
  if (ofs_.fail()) {
    throw Exception("unable to open '" + filename + "': " +  std::strerror(errno));
  }
}

StreamLogger&
operator<<(StreamLogger& logger, std::ostream& (*os)(std::ostream&))
{
  logger.out_stream_ << os;
  return logger;
}

StreamLogger&
operator<<(const std::shared_ptr<StreamLogger>& logger, std::ostream& (*os)(std::ostream&))
{
  logger->out_stream_ << os;
  return *logger;
}

void
StreamLogger::Log(const std::string& msg, size_t indent, bool endline)
{
  static std::mutex mu;
  std::lock_guard<std::mutex> lk(mu);

  if (indent == 0) {
    out_stream_ << msg;
  }
  else {
    out_stream_ << IndentMultiline(msg, indent);
  }

  if (endline) {
    out_stream_ << std::endl;
  }
}

} // namespace common
