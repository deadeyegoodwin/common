// Copyright 2023, David Goodwin. All rights reserved.

#include <fstream>
#include <iostream>
#include <string>
#include "common_json_preprocessor.h"

namespace {

#define CheckTrueMsg(P, MSG)                                            \
  do {                                                                  \
    const bool p__ = (P);                                               \
    if (!p__) {                                                         \
      std::cerr << std::string("error: ") << (MSG) << std::endl;        \
      exit(1);                                                          \
    }                                                                   \
  } while (false)

#define Fail(MSG)                                                     \
  do {                                                                \
    std::cerr << "error: " << (MSG) << std::endl;                     \
    exit(1);                                                          \
  } while (false)

} // namespace

// json_preprocessor_test <input_json> <expected_output_json>
// Writes output to stdout
int
main(int argc, char **argv)
{
  try {

    // Echo command line
    for (auto i = 0; i < argc; ++i) {
      std::cout << argv[i] << " ";
    }
    std::cout << std::endl;

    if (argc < 3) {
      std::cerr << "usage: json_preprocessor_test <input_json> <expected_output_json>" << std::endl;
      exit(1);
    }

    const std::string input_json(argv[1]);
    const std::string expected_output_json(argv[2]);

    std::ifstream ifs(input_json);
    CheckTrueMsg(!ifs.fail(), std::string("unable to read '") + input_json + "'");

    std::ifstream efs(expected_output_json);
    CheckTrueMsg(!efs.fail(), std::string("unable to read '") + expected_output_json + "'");

    json j = json::parse(ifs);
    common::JsonPreprocessor preprocessor(j);
    const json& preprocessed = preprocessor.PreprocessedJson();

    auto expected = json::parse(efs);
    if (preprocessed != expected) {
      std::stringstream ss;
      ss << "expected:" << std::endl;
      ss << std::setw(2) << expected << std::endl;
      ss << "actual:" << std::endl;
      ss << std::setw(2) << preprocessed << std::endl;
      std::cerr << ss.str() << std::endl;
      exit(1);
    }

  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << std::endl;
    exit(1);
  }

  return 0;
}
