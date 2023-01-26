// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

#include <nlohmann/json.hpp>
#include <unordered_map>

using json = nlohmann::json;

namespace common {

//
// Settings
//
// Settings cannot be changed except by FromJson and FromJsonString.
//
class Settings {
public:
  // Indicate a setting name is allowed and the default value.
  void AllowedSetting(const std::string& name, bool def);
  void AllowedSetting(const std::string& name, int32_t def);
  void AllowedSetting(const std::string& name, float def);
  void AllowedSetting(const std::string& name, const std::string& def);
  void AllowedSetting(const std::string& name, const char* def);

  // Initialize from JSON. Error if JSON specifies a non-allowed
  // setting name of datatype.
  void FromJson(const json& j);
  void FromJsonString(const std::string& js);

  // Return the number of settings.
  size_t Count() const;

  // Exception if setting is not found.
  void Setting(const std::string& name, bool& v) const;
  void Setting(const std::string& name, int32_t& v) const;
  void Setting(const std::string& name, float& v) const;
  void Setting(const std::string& name, std::string& v) const;
  bool BooleanSetting(const std::string& name) const;
  int32_t IntegerSetting(const std::string& name) const;
  float FloatSetting(const std::string& name) const;
  const std::string& StringSetting(const std::string& name) const;

  void ToJson(json& j) const;
  std::string ToString() const;

protected:
  void UpdateSetting(const std::string& name, const bool v);
  void UpdateSetting(const std::string& name, const int32_t v);
  void UpdateSetting(const std::string& name, const float v);
  void UpdateSetting(const std::string& name, const std::string& v);
  void UpdateSetting(const std::string& name, const char* v);

  std::unordered_map<std::string, bool> boolean_settings_;
  std::unordered_map<std::string, int32_t> integer_settings_;
  std::unordered_map<std::string, float> float_settings_;
  std::unordered_map<std::string, std::string> string_settings_;
};

//
// MutableSettings
//
// Like Settings except settings can be changed with SetSetting
// functions.
//
class MutableSettings : public Settings {
public:
  void SetSetting(const std::string& name, const bool v);
  void SetSetting(const std::string& name, const int32_t v);
  void SetSetting(const std::string& name, const float v);
  void SetSetting(const std::string& name, const std::string& v);
  void SetSetting(const std::string& name, const char* v);
};

} // namespace common
