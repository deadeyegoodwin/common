// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace common {

// Utility method to indent every line of a string.
std::string
IndentMultiline(const std::string& str, const size_t indent);

//
// Logger
//
class Logger {
public:
  virtual ~Logger() = default;

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

//
// StdoutLogger
//
class StdoutLogger : public Logger {
public:
  virtual void Log(const std::string& msg, size_t indent=0, bool endline=false) const;
};

template <class T>
StdoutLogger& operator<<(StdoutLogger& logger, const T& t)
{
  std::cout << t;
  return logger;
}

template <class T>
StdoutLogger& operator<<(const std::shared_ptr<StdoutLogger>& logger, const T& t)
{
  std::cout << t;
  return *logger;
}

// Handle std::endl
StdoutLogger& operator<<(StdoutLogger& logger, std::ostream& (*os)(std::ostream&));
StdoutLogger& operator<<(const std::shared_ptr<StdoutLogger>& logger, std::ostream& (*os)(std::ostream&));

} // namespace common
