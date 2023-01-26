// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace metis {

// Utility method to indent every line of a string.
std::string
IndentMultiline(const std::string& str, const size_t indent);

//
// MetisLogger
//
class MetisLogger {
public:
  bool IsQuietEnabled() const { return quiet_; }
  bool IsInfoEnabled() const { return !quiet_; }
  bool IsVerboseEnabled() const { return verbose_; }
  bool IsDebugEnabled() const { return debug_; }

  void EnableQuiet() { quiet_ = true; verbose_ = false; debug_ = false; }
  void EnableInfo() { quiet_ = false; verbose_ = false; debug_ = false; }
  void EnableVerbose() { quiet_ = false; verbose_ = true; debug_ = false; }
  void EnableDebug() { quiet_ = false; verbose_ = true; debug_ = true; }

  virtual void Log(const std::string& msg, size_t indent=0, bool endline=true) const = 0;

  void LogInfo(const std::string& msg, size_t indent=0, bool endline=true) const {
    if (IsInfoEnabled()) { Log(msg, indent, endline); }
  }
  void LogVerbose(const std::string& msg, size_t indent=0, bool endline=true) const {
    if (IsVerboseEnabled()) { Log(msg, indent, endline); }
  }
  void LogDebug(const std::string& msg, size_t indent=0, bool endline=true) const {
    if (IsDebugEnabled()) { Log(msg, indent, endline); }
  }

private:
  bool quiet_ { false };
  bool verbose_ { false };
  bool debug_ { false };
};

template <class T>
MetisLogger& operator<<(MetisLogger& logger, const T& t)
{
  std::cout << t;
  return logger;
}

template <class T>
MetisLogger& operator<<(const std::shared_ptr<MetisLogger>& logger, const T& t)
{
  std::cout << t;
  return *logger;
}

// Handle std::endl
MetisLogger& operator<<(MetisLogger& logger, std::ostream& (*os)(std::ostream&));
MetisLogger& operator<<(const std::shared_ptr<MetisLogger>& logger, std::ostream& (*os)(std::ostream&));

//
// MetisStdoutLogger
//
class MetisStdoutLogger : public MetisLogger {
public:
  virtual void Log(const std::string& msg, size_t indent=0, bool endline=false) const;
};

} // namespace metis
