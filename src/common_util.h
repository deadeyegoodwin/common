// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

#include <algorithm>
#include <cctype>
#include <functional>
#include <locale>
#include <string>

namespace common {

//
// String utilities
//
static inline std::string& ToLower(std::string &s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c){ return std::tolower(c); });
  return s;
}

static inline std::string& ToUpper(std::string &s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c){ return std::toupper(c); });
  return s;
}

static inline std::string& TrimLeft(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                  std::not1(std::ptr_fun<int, int>(std::isspace))));
  return s;
}

static inline std::string TrimLeft(std::string &&sm) {
  std::string s = std::move(sm);
  s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                  std::not1(std::ptr_fun<int, int>(std::isspace))));
  return s;
}

static inline std::string& TrimRight(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(),
                       std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
  return s;
}

static inline std::string TrimRight(std::string &&sm) {
  std::string s = std::move(sm);
  s.erase(std::find_if(s.rbegin(), s.rend(),
                       std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
  return s;
}

static inline std::string& Trim(std::string &s) {
  return TrimLeft(TrimRight(s));
}

static inline std::string Trim(std::string &&sm) {
  return TrimLeft(std::move(TrimRight(sm)));
}

} // namespace common
