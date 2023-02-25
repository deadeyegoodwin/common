// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

#include <fstream>
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

  virtual void Log(const std::string& msg, size_t indent=0, bool endline=true) = 0;

  void LogInfo(const std::string& msg, size_t indent=0, bool endline=true) {
    if (IsInfoEnabled()) { Log(msg, indent, endline); }
  }
  void LogVerbose(const std::string& msg, size_t indent=0, bool endline=true) {
    if (IsVerboseEnabled()) { Log(msg, indent, endline); }
  }
  void LogDebug(const std::string& msg, size_t indent=0, bool endline=true) {
    if (IsDebugEnabled()) { Log(msg, indent, endline); }
  }

private:
  bool quiet_ { false };
  bool verbose_ { false };
  bool debug_ { false };
};

//
// StreamLogger
//
// Logs to std::cout, std::cerr or to a file.
//
class StreamLogger : public Logger {
public:
  // Logger holds a reference to 'of' so the caller is required to
  // ensure that the lifetime of 'of' extends over the lifetime of
  // 'this' logger.
  StreamLogger(std::ostream& of);

  // Open named file and stream logging to that file.
  StreamLogger(const std::string& filename);

  virtual void Log(const std::string& msg, size_t indent=0, bool endline=true);

private:
  friend StreamLogger& operator<<(StreamLogger& logger, std::ostream& (*os)(std::ostream&));
  friend StreamLogger& operator<<(const std::shared_ptr<StreamLogger>& logger,
                                  std::ostream& (*os)(std::ostream&));
  std::ofstream ofs_;
  std::ostream& out_stream_;
};

template <class T>
StreamLogger& operator<<(StreamLogger& logger, const T& t)
{
  logger.out_stream_ << t;
  return logger;
}

template <class T>
StreamLogger& operator<<(const std::shared_ptr<StreamLogger>& logger, const T& t)
{
  logger->out_stream_ << t;
  return *logger;
}

// Handle std::endl
StreamLogger& operator<<(StreamLogger& logger, std::ostream& (*os)(std::ostream&));
StreamLogger& operator<<(const std::shared_ptr<StreamLogger>& logger, std::ostream& (*os)(std::ostream&));

} // namespace common
