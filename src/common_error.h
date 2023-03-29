// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

#include <exception>
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

} // namespace common
