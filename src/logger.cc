// Copyright 2023, David Goodwin. All rights reserved.

#include <mutex>
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

StdoutLogger&
operator<<(StdoutLogger& logger, std::ostream& (*os)(std::ostream&))
{
  std::cout << os;
  return logger;
}

StdoutLogger&
operator<<(const std::shared_ptr<StdoutLogger>& logger, std::ostream& (*os)(std::ostream&))
{
  std::cout << os;
  return *logger;
}

void
StdoutLogger::Log(const std::string& msg, size_t indent, bool endline) const
{
  static std::mutex mu;
  std::lock_guard<std::mutex> lk(mu);

  if (indent == 0) {
    std::cout << msg;
  }
  else {
    std::cout << IndentMultiline(msg, indent);
  }

  if (endline) {
    std::cout << std::endl;
  }
}

} // namespace common
