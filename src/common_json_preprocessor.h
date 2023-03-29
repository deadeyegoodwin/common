// Copyright 2023, David Goodwin. All rights reserved.
#pragma once

#include <nlohmann/json.hpp>
#include <unordered_map>

using json = nlohmann::json;

namespace common {

//
// JsonPreprocessor
//
// Preprocess json to substitute named "preprocessor-templates" json
// into references indicated by "preprocessor-template".
//
class JsonPreprocessor {
public:
  // Preprocess a json object or json string.
  JsonPreprocessor(const json& j);
  JsonPreprocessor(const std::string& js);

  // Return the original json.
  const json& OriginalJson() const { return original_json_; }

  // Return the pre-processed json.
  const json& PreprocessedJson();

private:
  void ProcessTemplates(const json& j);
  void Process(json& j, size_t rec_count);

  bool needs_preprocess_;
  const json original_json_;

  std::unordered_map<std::string, const json> templates_;

  // Contains the preprocessed json if 'needs_preprocess_' is
  // false. Contains the json that needs to be preprocessed if
  // 'needs_preprocess_' is true.
  json preprocess_json_;
};

} // namespace common
