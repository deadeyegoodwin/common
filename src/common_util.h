// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <locale>
#include <string>
#include <vector>

#include "common_error.h"

namespace common {

//
// String utilities
//

// In-place to-lowercase.
static inline std::string& ToLower(std::string &s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c){ return std::tolower(c); });
  return s;
}

// In-place to-uppercase.
static inline std::string& ToUpper(std::string &s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c){ return std::toupper(c); });
  return s;
}

// In-place left trim.
static inline std::string& TrimLeft(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](char c) { return !std::isspace(c); }));
  return s;
}

// In-place left trim.
static inline std::string TrimLeft(std::string &&sm) {
  std::string s = std::move(sm);
	return common::TrimLeft(s);
}

// In-place right trim.
static inline std::string& TrimRight(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(), [](char c) { return !std::isspace(c); }).base(), s.end());
  return s;
}

// In-place right trim.
static inline std::string TrimRight(std::string &&sm) {
  std::string s = std::move(sm);
	return common::TrimRight(s);
}

// In-place trim.
static inline std::string& Trim(std::string &s) {
  return TrimLeft(TrimRight(s));
}

// In-place trim.
static inline std::string Trim(std::string &&sm) {
  return TrimLeft(std::move(TrimRight(sm)));
}

// Split a string into multiple newline-separated ('\n') lines where
// each line is at most 'width' characters wide. Will attempt to split
// the line at space (' ') character but if that is not possible will
// insert a hyphen ('-'). String is modified in place.
void Split(std::string& s, const size_t width);

// Parse string representation of delimiter separated list of signed
// integers into a vector<int64_t>. Throws common::Exception if the
// string representation cannot be parsed into signed integers.
std::vector<int64_t> StringToSignedIntegerVector(std::string s, const std::string& delimiter);

// Parse string representation of delimiter separated list of signed
// integers into a vector<uint64_t>. Throws common::Exception if the
// string representation cannot be parsed as signed integers or if any
// of the parsed integers is negative.
std::vector<uint64_t> StringToUnsignedIntegerVector(std::string s, const std::string& delimiter);

} // namespace common
