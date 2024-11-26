// Copyright 2023, David Goodwin. All rights reserved.

#include "common_error.h"
#include "common_util.h"

namespace common {

std::string
Join(const std::vector<size_t>& els, const std::string& delimiter)
{
  std::string str;
  bool first = true;
  for (const auto& s : els) {
    if (!first) {
      str += delimiter;
    }

    str += std::to_string(s);
    first = false;
  }

  return str;
}

std::string
Join(const std::vector<std::string>& els, const std::string& delimiter)
{
  std::string str;
  bool first = true;
  for (const auto& s : els) {
    if (!first) {
      str += delimiter;
    }

    str += s;
    first = false;
  }

  return str;
}

void
Split(std::string& s, const size_t width)
{
  THROWIF(width <= 1, "minimum string split width is 2");

  size_t current_pos = 0;
  while ((current_pos + width) < s.length()) {
    size_t n = s.rfind(' ', current_pos + width);
    if ((n != std::string::npos) && (n >= current_pos)) {
      s.replace(n, 1, "\n");
      current_pos = n + 1;
    } else {
      s.insert(current_pos + width - 1, "-\n");
      current_pos += width + 1;
    }
  }
}

void
Indent(std::string& s, const std::vector<size_t> indents, bool indent_empty_lines)
{
  THROWIF(indents.empty(), "at least 1 indent is required");

  size_t indents_idx = 0;
  size_t pos = 0;

  do {
    if (indent_empty_lines || ((pos < s.length()) && (s[pos] != '\n'))) {
      s.insert(pos, indents[indents_idx], ' ');
    }

    if (indents_idx < (indents.size() - 1)) {
      indents_idx += 1;
    }

    pos = s.find('\n', pos);
    if ((pos == std::string::npos) || (pos >= (s.length() - 1))) {
      break;
    }
    pos += 1; // strlen('\n')
  } while (true);
}

std::string
DuplicateAndIndent(const std::string& s, const std::vector<size_t> indents, bool indent_empty_lines)
{
  std::string str(s);
  Indent(str, indents, indent_empty_lines);
  return str;
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
  for (int64_t svi : sv) {
    THROWIF(svi < 0, "negative integers not allowed");
    v.push_back(uint64_t(svi));
  }

  return v;
}

} // namespace common
