// Copyright 2023, David Goodwin. All rights reserved.

#include "common_error.h"
#include "common_json_preprocessor.h"

namespace common {

JsonPreprocessor::JsonPreprocessor(const json& j) :
  original_json_(j)
{
  // json only needs preprocessing if it is an object and contains
  // 'preprocessor-templates' and 'body' elements.
  needs_preprocess_ =
    (j.is_object() && j.contains("preprocessor-templates") && j.contains("body"));

  if (!needs_preprocess_) {
    preprocess_json_ = original_json_;
  }
  else {
    ProcessTemplates(j["preprocessor-templates"]);
    preprocess_json_ = j["body"];
  }
}

JsonPreprocessor::JsonPreprocessor(const std::string& js) :
  JsonPreprocessor(json::parse(js))
{
}

const json&
JsonPreprocessor::PreprocessedJson()
{
  if (needs_preprocess_) {
    Process(preprocess_json_, templates_.size() * 2);
    needs_preprocess_ = false;
  }

  return preprocess_json_;
}

void
JsonPreprocessor::ProcessTemplates(const json& j)
{
  for (auto& it : j.items()) {
    const std::string& name = it.key();
    const json& jt = it.value();

    if (templates_.find(name) != templates_.end()) {
      throw common::Exception("duplicate template name '" + name + "'");
    }

    templates_.emplace(name, jt);
  }
}

void
JsonPreprocessor::Process(json& j, size_t rec_count)
{
  // 'rec_count' is used to detect a cycle in the templates.
  if (rec_count == 0) {
    throw common::Exception("preprocessor failed, likely due to circular template dependencies");
  }

  // If 'j' is an object that contains the 'preprocessor-template'
  // element, replace that object with a copy of the template and then
  // recursively revisit the "new" 'j'.
  if (j.is_object() && j.contains("preprocessor-template")) {
    for (auto& it : j.items()) {
      if ((it.key() != "preprocessor-template") &&
          (it.key() != "merge") && (it.key() != "overwrite")) {
        throw common::Exception(
          std::string("preprocessor object can only contain 'preprocessor-template', ") +
          "'merge', and 'overwrite' members, found '" + it.key() + "'");
      }
    }

    const std::string& template_name = j["preprocessor-template"];
    if (templates_.find(template_name) == templates_.end()) {
      throw common::Exception("unknown template '" + template_name + "'");
    }

    json jnew(templates_[template_name]);
    Process(jnew, rec_count - 1);

    // 'jnew' now has the completely preprocessed version of
    // 'j'. Apply "merge" and "overwrite" if present.
    if (j.contains("overwrite")) {
      jnew.update(j["overwrite"], false /* merge_objects */);
    }
    if (j.contains("merge")) {
      jnew.update(j["merge"], true /* merge_objects */);
    }

    j.swap(jnew);
  }

  // If 'j' is an object (already preprocessed because of above), then
  // visit each member and process any preprocessing.
  if (j.is_object()) {
    for (auto& it : j.items()) {
      Process(it.value(), rec_count);
    }
  }
}

} // namespace common
