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
  enum class Level {
    OFF = 0,  // Suppress all logging
    LOW = 1,  // Show only LOW volume logging
    MED = 2,  // Show LOW and MED volume logging
    HIGH = 3  // Show LOW, MED and HIGH volume logging
  };

  Logger() : filter_level_(Level::LOW) { }
  Logger(const Level filter_level) : filter_level_(filter_level) { }
  Logger(const Logger& other) = default;
  virtual ~Logger() = default;

  // Get or set the volume filter. A log request will be surpressed if
  // it is at a level higher than FilterLevel().
  Level FilterLevel() const { return filter_level_; }
  void SetFilterLevel(const Level filter_level) { filter_level_ = filter_level; }

  // Log a message if 'fitler_level' is no greater than FilterLevel().
  void Log(Level filter_level, const std::string& msg, size_t indent=0, bool endline=true) {
    if ((filter_level != Level::OFF) && (filter_level <= filter_level_)) {
      Log(msg, indent, endline);
    }
  }

  // Log a low volume message. Log unless FilterLevel == OFF.
  void LogLow(const std::string& msg, size_t indent=0, bool endline=true) {
    Log(Level::LOW, msg, indent, endline);
  }

  // Log a medium volume message. Log if FilterLevel is MED or HIGH.
  void LogMed(const std::string& msg, size_t indent=0, bool endline=true) {
    Log(Level::MED, msg, indent, endline);
  }

  // Log a high volume message. Log only if FilterLevel is HIGH.
  void LogHigh(const std::string& msg, size_t indent=0, bool endline=true) {
    Log(Level::HIGH, msg, indent, endline);
  }

  virtual void Log(const std::string& msg, size_t indent=0, bool endline=true) = 0;

protected:
  Level filter_level_;
};

//
// Logging defines.
//
// These avoid the evaluation of the message expression if the message
// is not going to be logger.
//
#define LOGGER_LOGLOW(L, M)                                             \
  do {                                                                  \
    common::Logger& logger__ = (L);                                     \
    if (logger__.FilterLevel() >= common::Logger::Level::LOW) {         \
      logger__.Log(M);                                                  \
    }                                                                   \
  } while (false)

#define LOGGER_LOGMED(L, M)                                             \
  do {                                                                  \
    common::Logger& logger__ = (L);                                     \
    if (logger__.FilterLevel() >= common::Logger::Level::MED) {         \
      logger__.Log(M);                                                  \
    }                                                                   \
  } while (false)

#define LOGGER_LOGHIGH(L, M)                                            \
  do {                                                                  \
    common::Logger& logger__ = (L);                                     \
    if (logger__.FilterLevel() >= common::Logger::Level::HIGH) {        \
      logger__.Log(M);                                                  \
    }                                                                   \
  } while (false)

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
  StreamLogger(Logger::Level filter_level, std::ostream& of);

  // Open named file and stream logging to that file.
  StreamLogger(Logger::Level filter_level, const std::string& filename, bool buffered = true);

  // Copy settings from existing logger, but override any existing
  // logging destination to instead log to provided ostream or file.
  StreamLogger(const Logger& other, std::ostream& of);
  StreamLogger(const Logger& other, const std::string& filename, bool buffered = true);

  virtual void Log(const std::string& msg, size_t indent=0, bool endline=true) override;

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
