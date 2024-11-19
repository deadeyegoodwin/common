// Copyright 2023, David Goodwin. All rights reserved.

#include "common_error.h"
#include "common_util.h"

namespace common {

void
Split(std::string& s, const size_t width)
{
  THROWIF(width <= 1, "minimum string split width is 2");

  size_t current_pos = 0;
  while ((current_pos + width) < s.length()) {
    int n = s.rfind(' ', current_pos + width);
    if ((n != int(std::string::npos)) && (n >= (int)current_pos)) {
      s.replace(n, 1, "\n");
      current_pos = n + 1;
    } else {
      s.insert(current_pos + width - 1, "-\n");
      current_pos += width + 1;
    }
  }
}

std::vector<int64_t>
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

std::vector<uint64_t>
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
