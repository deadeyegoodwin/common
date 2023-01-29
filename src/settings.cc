// Copyright 2023, David Goodwin. All rights reserved.

#include <cstdlib>
#include <sstream>
#include "error.h"
#include "settings.h"

namespace common {

void
Settings::AllowedSetting(const std::string& name, bool def)
{
  const auto& it = boolean_settings_.find(name);
  if (it != boolean_settings_.end()) {
    throw Exception("setting initialized multiple times: '" + name + "'");
  }

  boolean_settings_[name] = def;
}

void
Settings::AllowedSetting(const std::string& name, int32_t def)
{
  const auto& it = integer_settings_.find(name);
  if (it != integer_settings_.end()) {
    throw Exception("setting initialized multiple times: '" + name + "'");
  }

  integer_settings_[name] = def;
}

void
Settings::AllowedSetting(const std::string& name, float def)
{
  const auto& it = float_settings_.find(name);
  if (it != float_settings_.end()) {
    throw Exception("setting initialized multiple times: '" + name + "'");
  }

  float_settings_[name] = def;
}

void
Settings::AllowedSetting(const std::string& name, const std::string& def)
{
  const auto& it = string_settings_.find(name);
  if (it != string_settings_.end()) {
    throw Exception("setting initialized multiple times: '" + name + "'");
  }

  string_settings_[name] = def;
}

void
Settings::AllowedSetting(const std::string& name, const char* def)
{
  const auto& it = string_settings_.find(name);
  if (it != string_settings_.end()) {
    throw Exception("setting initialized multiple times: '" + name + "'");
  }

  string_settings_[name] = std::string(def);
}

void
Settings::AllowedNonSetting(const std::string& name)
{
  const auto& it = non_settings_.find(name);
  if (it != non_settings_.end()) {
    throw Exception("non-setting initialized multiple times: '" + name + "'");
  }

  non_settings_.insert(name);
}

void
Settings::FromJson(const json& j)
{
  if (!j.is_object()) {
    throw Exception("settings must be contained in JSON object");
  }

  for (auto& it : j.items()) {
    const std::string& name = it.key();

    const auto& ns_it = non_settings_.find(name);
    if (ns_it != non_settings_.end()) {
      continue;
    }

    if (it.value().is_boolean()) {
      bool v; it.value().get_to(v); UpdateSetting(name, v);
    }
    else if (it.value().is_number_integer()) {
      int32_t v; it.value().get_to(v); UpdateSetting(name, v);
    }
    else if (it.value().is_number_float()) {
      float v; it.value().get_to(v); UpdateSetting(name, v);
    }
    else if (it.value().is_string()) {
      std::string v; it.value().get_to(v); UpdateSetting(name, v);
    }
    else {
      throw Exception("unexpected setting '" + name + "'");
    }
  }
}

void
Settings::FromJsonString(const std::string& js)
{
  FromJson(json::parse(js));
}

size_t
Settings::Count() const
{
  return boolean_settings_.size() + integer_settings_.size() + float_settings_.size() + string_settings_.size();
}

void
Settings::Setting(const std::string& name, bool& v) const
{
  const auto& it = boolean_settings_.find(name);
  if (it == boolean_settings_.end()) {
    throw Exception("unknown boolean setting '" + name + "'");
  }

  v = it->second;
}

void
Settings::Setting(const std::string& name, int32_t& v) const
{
  const auto& it = integer_settings_.find(name);
  if (it == integer_settings_.end()) {
    throw Exception("unknown integer setting '" + name + "'");
  }

  v = it->second;
}

void
Settings::Setting(const std::string& name, uint32_t& v) const
{
  const auto& it = integer_settings_.find(name);
  if (it == integer_settings_.end()) {
    throw Exception("unknown integer setting '" + name + "'");
  }
  if (it->second < 0) {
    throw Exception("expecting integer setting '" + name + "' to be >= 0");
  }

  v = (uint32_t)it->second;
}

void
Settings::Setting(const std::string& name, size_t& v) const
{
  const auto& it = integer_settings_.find(name);
  if (it == integer_settings_.end()) {
    throw Exception("unknown integer setting '" + name + "'");
  }
  if (it->second < 0) {
    throw Exception("expecting integer setting '" + name + "' to be >= 0");
  }

  v = (size_t)it->second;
}

void
Settings::Setting(const std::string& name, float& v) const
{
  const auto& it = float_settings_.find(name);
  if (it == float_settings_.end()) {
    throw Exception("unknown float setting '" + name + "'");
  }

  v = it->second;
}

void
Settings::Setting(const std::string& name, std::string& v) const
{
  const auto& it = string_settings_.find(name);
  if (it == string_settings_.end()) {
    throw Exception("unknown string setting '" + name + "'");
  }

    v = it->second;
}

bool
Settings::BooleanSetting(const std::string& name) const
{
  const auto& it = boolean_settings_.find(name);
  if (it == boolean_settings_.end()) {
    throw Exception("unknown boolean setting '" + name + "'");
  }

  return it->second;
}

int32_t
Settings::IntegerSetting(const std::string& name) const
{
  const auto& it = integer_settings_.find(name);
  if (it == integer_settings_.end()) {
    throw Exception("unknown integer setting '" + name + "'");
  }

    return it->second;
}

float
Settings::FloatSetting(const std::string& name) const
{
  const auto& it = float_settings_.find(name);
  if (it == float_settings_.end()) {
    throw Exception("unknown float setting '" + name + "'");
  }

  return it->second;
}

const std::string&
Settings::StringSetting(const std::string& name) const
{
  const auto& it = string_settings_.find(name);
  if (it == string_settings_.end()) {
    throw Exception("unknown string setting '" + name + "'");
  }

  return it->second;
}

void
Settings::UpdateSetting(const std::string& name, const bool v)
{
  const auto& it = boolean_settings_.find(name);
  if (it == boolean_settings_.end()) {
    throw Exception("unable to set unknown boolean setting '" + name + "'");
  }

  it->second = v;
}

void
Settings::UpdateSetting(const std::string& name, const int32_t v)
{
  const auto& it = integer_settings_.find(name);
  if (it == integer_settings_.end()) {
    throw Exception("unable to set unknown integer setting '" + name + "'");
  }

  it->second = v;
}

void
Settings::UpdateSetting(const std::string& name, const uint32_t v)
{
  const auto& it = integer_settings_.find(name);
  if (it == integer_settings_.end()) {
    throw Exception("unable to set unknown integer setting '" + name + "'");
  }
  if (v > std::numeric_limits<int32_t>::max()) {
    throw Exception("out-of-range value for integer setting '" + name + "'");
  }

  it->second = (int32_t)v;
}

void
Settings::UpdateSetting(const std::string& name, const size_t v)
{
  const auto& it = integer_settings_.find(name);
  if (it == integer_settings_.end()) {
    throw Exception("unable to set unknown integer setting '" + name + "'");
  }
  if (v > std::numeric_limits<int32_t>::max()) {
    throw Exception("out-of-range value for integer setting '" + name + "'");
  }

  it->second = (size_t)v;
}

void
Settings::UpdateSetting(const std::string& name, const float v)
{
  const auto& it = float_settings_.find(name);
  if (it == float_settings_.end()) {
    throw Exception("unable to set unknown float setting '" + name + "'");
  }

    it->second = v;
}

void
Settings::UpdateSetting(const std::string& name, const std::string& v)
{
  const auto& it = string_settings_.find(name);
  if (it == string_settings_.end()) {
    throw Exception("unable to set unknown string setting '" + name + "'");
  }

  it->second = v;
}

void
Settings::UpdateSetting(const std::string& name, const char* v)
{
  const auto& it = string_settings_.find(name);
  if (it == string_settings_.end()) {
    throw Exception("unable to set unknown string setting '" + name + "'");
  }

  it->second = std::string(v);
}

void
Settings::ToJson(json& j) const
{
  if (!j.is_object()) {
    throw Exception("settings must be added to a JSON object");
  }

  for (auto& pr : boolean_settings_) {
    j[pr.first] = pr.second;
  }
  for (auto& pr : integer_settings_) {
    j[pr.first] = pr.second;
  }
  for (auto& pr : float_settings_) {
    j[pr.first] = pr.second;
  }
  for (auto& pr : string_settings_) {
    j[pr.first] = pr.second;
  }
}

std::string
Settings::ToString() const
{
  std::stringstream sstr;
  bool first = false;

  for (auto& pr : boolean_settings_) {
    if (first) { sstr << std::endl; }
    sstr << pr.first << " : " << (pr.second ? "true" : "false");
    first = true;
  }

  for (auto& pr : integer_settings_) {
    if (first) { sstr << std::endl; }
    sstr << pr.first << " : " << pr.second;
    first = true;
  }

  for (auto& pr : float_settings_) {
    if (first) { sstr << std::endl; }
    sstr << pr.first << " : " << pr.second;
    first = true;
  }

  for (auto& pr : string_settings_) {
    if (first) { sstr << std::endl; }
    sstr << pr.first << " : " << pr.second;
    first = true;
  }

  return sstr.str();
}

void
MutableSettings::SetSetting(const std::string& name, const bool v)
{
  UpdateSetting(name, v);
}

void
MutableSettings::SetSetting(const std::string& name, const int32_t v)
{
  UpdateSetting(name, v);
}

void
MutableSettings::SetSetting(const std::string& name, const uint32_t v)
{
  UpdateSetting(name, v);
}

void
MutableSettings::SetSetting(const std::string& name, const size_t v)
{
  UpdateSetting(name, v);
}

void
MutableSettings::SetSetting(const std::string& name, const float v)
{
  UpdateSetting(name, v);
}

void
MutableSettings::SetSetting(const std::string& name, const std::string& v)
{
  UpdateSetting(name, v);
}

void
MutableSettings::SetSetting(const std::string& name, const char* v)
{
  UpdateSetting(name, v);
}

} // namespace common
