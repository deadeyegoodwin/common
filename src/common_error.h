// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

#include <exception>
#include <sstream>
#include <string>

namespace common {

//
// Exception
//
class Exception : public std::exception
{
public:
  Exception(const std::string& s) : msg_(s) { }
  Exception(const char* s) : msg_(s) { }
  const char* what() const noexcept { return msg_.c_str(); }

private:
  const std::string msg_;
};

//
// Exception defines.
//
// These avoid the evaluation of the message expression if exception
// is not thrown.
//
#define THROWIF(P, M)                                                   \
  do {                                                                  \
    const bool p = (P);                                                 \
    if (p) {                                                            \
      std::stringstream sstr;                                           \
      sstr << __FILE__ << ':' << __LINE__ << " " << (M);                \
      throw common::Exception(sstr.str());                              \
    }                                                                   \
  } while (false)

} // namespace common
