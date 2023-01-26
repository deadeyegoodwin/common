// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

#include <exception>
#include <string>

namespace metis {

//
// MetisException
//
class MetisException : public std::exception
{
public:
  MetisException(const std::string& s) : msg_(s) { }
  MetisException(const char* s) : msg_(s) { }
  const char* what() const noexcept { return msg_.c_str(); }

private:
  const std::string msg_;
};

} // namespace metis
