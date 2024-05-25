// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

#include <algorithm>
#include <cctype>
#include <functional>
#include <locale>
#include <string>
#include <vector>

#include "common_error.h"

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

// Parse string representation of delimiter separated list of signed
// integers into a vector<int64_t>. Throws common::Exception if the
// string representation cannot be parsed into signed integers.
static inline std::vector<int64_t>
StringToSignedIntegerVector(std::string s, const std::string& delimiter)
{
  std::vector<int64_t> v;
  size_t spos = 0, epos = 0;
  while ((epos = s.find(delimiter, spos)) != std::string::npos) {
    std::string token = s.substr(spos, epos);
    Trim(token);
    THROWIF(token.empty(), "empty string cannot be converted to integer");

    std::string::size_type idx;
    long long ll = std::stoll(token, &idx);
    THROWIF(idx != token.size(), "unexpected non-integer characters");

    v.push_back(ll);
    spos = epos + delimiter.size();
  }

  std::string token = s.substr(spos);
  Trim(token);
  THROWIF(token.empty(), "empty string cannot be converted to integer");

  std::string::size_type idx;
  long long ll = std::stoll(token, &idx);
  THROWIF(idx != token.size(), "unexpected non-integer characters");
  v.push_back(ll);

  return v;
}

// Parse string representation of delimiter separated list of signed
// integers into a vector<uint64_t>. Throws common::Exception if the
// string representation cannot be parsed as signed integers or if any
// of the parsed integers is negative.
static inline std::vector<uint64_t>
StringToUnsignedIntegerVector(std::string s, const std::string& delimiter)
{
  std::vector<uint64_t> v;
  auto sv = StringToSignedIntegerVector(s, delimiter);
  for (long long svi : sv) {
    THROWIF(svi < 0, "negative integers not allowed");
    v.push_back(uint64_t(svi));
  }

  return v;
}

} // namespace common
