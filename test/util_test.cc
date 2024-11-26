// Copyright 2023, David Goodwin. All rights reserved.

#include <iostream>
#include <string>
#include <vector>
#include "common_error.h"
#include "common_util.h"

namespace {

#define Fail(MSG)                                                     \
  do {                                                                \
    std::cerr << "error: " << (MSG) << std::endl;                     \
    exit(1);                                                          \
  } while (false)

} // namespace

// util_test
int
main(int argc, char **argv)
{
  // Echo command line
  for (auto i = 0; i < argc; ++i) {
    std::cout << argv[i] << " ";
  }
  std::cout << std::endl;

  try {

    // Test string upper/lower
    {
      std::vector<std::tuple<std::string, std::string, std::string>> tul {
        { "", "", "" },
        { " ", " ", " " },
        { "\t", "\t", "\t" },
        { "1234567890", "1234567890", "1234567890" },
        { "abc", "abc", "ABC" },
        { "ABC", "abc", "ABC" },
        { "AbC", "abc", "ABC" },
        { "AbC\ndEf", "abc\ndef", "ABC\nDEF" }
      };

      for (auto& s : tul) {
        std::string lstr(std::get<0>(s));
        common::ToLower(lstr);
        if (lstr != std::get<1>(s)) {
          Fail("unexpected to-lower of '" + std::get<0>(s) + "', '" + lstr + "' != '" + std::get<1>(s) + "'");
        }

        std::string ustr(std::get<0>(s));
        common::ToUpper(ustr);
        if (ustr != std::get<2>(s)) {
          Fail("unexpected to-upper of '" + std::get<0>(s) + "', '" + ustr + "' != '" + std::get<2>(s) + "'");
        }
      }
    }

    // Test string trim
    {
      std::vector<std::tuple<std::string, std::string, std::string, std::string>> ts {
        { "", "", "", "" },
        { " ", "", "", "" },
        { "  ", "", "", "" },
        { "\t", "", "", "" },
        { " \t", "", "", "" },
        { "123abc", "123abc", "123abc", "123abc" },
        { " 123abc", "123abc", "123abc", " 123abc" },
        { "123abc ", "123abc", "123abc ", "123abc" },
        { " 123 abc ", "123 abc", "123 abc ", " 123 abc" },
        { "\t 123\tabc ", "123\tabc", "123\tabc ", "\t 123\tabc" },
        { " 123 abc \t", "123 abc", "123 abc \t", " 123 abc" },
        { " 123abc\n", "123abc", "123abc\n", " 123abc" },
        { "\n 123\nabc ", "123\nabc", "123\nabc ", "\n 123\nabc" },
        { " 123 abc \n", "123 abc", "123 abc \n", " 123 abc" }
      };

      // Trim by reference...
      for (auto& s : ts) {
        std::string trim(std::get<0>(s));
        common::Trim(trim);
        if (trim != std::get<1>(s)) {
          Fail("unexpected trim of '" + std::get<0>(s) + "', '" + trim + "' != '" + std::get<1>(s) + "'");
        }

        std::string ltrim(std::get<0>(s));
        common::TrimLeft(ltrim);
        if (ltrim != std::get<2>(s)) {
          Fail("unexpected left-trim of '" + std::get<0>(s) + "', '" + ltrim + "' != '" + std::get<2>(s) + "'");
        }

        std::string rtrim(std::get<0>(s));
        common::TrimRight(rtrim);
        if (rtrim != std::get<3>(s)) {
          Fail("unexpected right-trim of '" + std::get<0>(s) + "', '" + rtrim + "' != '" + std::get<2>(s) + "'");
        }
      }

      // Trim with move...
      for (auto& s : ts) {
        std::string tstr(std::get<0>(s));
        std::string trim = common::Trim(std::move(tstr));
        if (trim != std::get<1>(s)) {
          Fail("unexpected trim of '" + std::get<0>(s) + "', '" + trim + "' != '" + std::get<1>(s) + "'");
        }

        std::string lstr(std::get<0>(s));
        std::string ltrim = common::TrimLeft(std::move(lstr));
        if (ltrim != std::get<2>(s)) {
          Fail("unexpected left-trim of '" + std::get<0>(s) + "', '" + ltrim + "' != '" + std::get<2>(s) + "'");
        }

        std::string rstr(std::get<0>(s));
        std::string rtrim = common::TrimRight(std::move(rstr));
        if (rtrim != std::get<3>(s)) {
          Fail("unexpected right-trim of '" + std::get<0>(s) + "', '" + rtrim + "' != '" + std::get<2>(s) + "'");
        }
      }
    }

    // Test string split
    {
      std::vector<std::tuple<std::string, size_t, std::string>> tss {
        { "", 2, "" },
        { " ", 2, " " },
        { "  ", 2, "  " },
        { "   ", 2, "  \n" },
        { "    ", 2, "  \n " },
        { "  \t", 2, " \n\t" },
        { "ab", 2, "ab" },
        { "abc", 2, "a-\nbc" },
        { "abcd", 3, "ab-\ncd" },
        { "abcde", 2, "a-\nb-\nc-\nde" },
        { "the rain in spain stays mainly in the plains", 10,
          "the rain\nin spain\nstays\nmainly in\nthe plains" },
        { "theraininspainstaysmainlyintheplains", 10,
          "therainin-\nspainstay-\nsmainlyin-\ntheplains" },
        { "the rain inspainstaysmainlyintheplains", 10,
          "the rain\ninspainst-\naysmainly-\nintheplai-\nns" }
      };

      for (auto& s : tss) {
        std::string ss(std::get<0>(s));
        common::Split(ss, std::get<1>(s));
        if (ss != std::get<2>(s)) {
          Fail("unexpected split (" + std::to_string(std::get<1>(s)) + ") of\n" +
               std::get<0>(s) + "\ngot:\n" + ss + "\nexpected:\n" + std::get<2>(s) + "\n");
        }
      }
    }

    // Test string indent (single indent value)
    {
      std::vector<std::tuple<std::string, size_t, std::string>> tsi {
        { "", 2, "  " },
        { "\n", 2, "  \n" },
        { "\n\n", 1, " \n \n" },
        { " \n\n ", 1, "  \n \n  " },
        { "\tabc def \n1234", 4, "    \tabc def \n    1234" },
        { "\tabc def \n1234\n", 4, "    \tabc def \n    1234\n" },
        { "\tabc def \n1234\nx", 4, "    \tabc def \n    1234\n    x" }
      };

      for (auto& s : tsi) {
        std::string ss(std::get<0>(s));
        common::Indent(ss, std::get<1>(s), true /* indent_empty_lines */);
        if (ss != std::get<2>(s)) {
          Fail("unexpected indent (" + std::to_string(std::get<1>(s)) + ") of\n" +
               std::get<0>(s) + "\ngot:\n'" + ss + "'\nexpected:\n'" + std::get<2>(s) + "'\n");
        }
      }
    }

    // Test string indent (single indent value, do not indent empty lines)
    {
      std::vector<std::tuple<std::string, size_t, std::string>> tsi {
        { "", 2, "" },
        { "\na", 2, "\n  a" },
        { "\n\nb", 1, "\n\n b" },
        { "c\nd\n", 1, " c\n d\n" },
        { "\n", 2, "\n" },
        { "\n\n", 1, "\n\n" },
        { " \n\n ", 1, "  \n\n  " },
        { "\n\n\n", 1, "\n\n\n" }
      };

      for (auto& s : tsi) {
        std::string ss(std::get<0>(s));
        common::Indent(ss, std::get<1>(s), false /* indent_empty_lines */);
        if (ss != std::get<2>(s)) {
          Fail("unexpected indent (" + std::to_string(std::get<1>(s)) + ") of\n" +
               std::get<0>(s) + "\ngot:\n'" + ss + "'\nexpected:\n'" + std::get<2>(s) + "'\n");
        }
      }
    }

    // Test string indent (per-line indent values)
    {
      std::vector<std::tuple<std::string, std::vector<size_t>, std::string>> tsli {
        { "abc def \n1234", {1, 2, 3}, " abc def \n  1234" },
        { "abc\n123\nxyz", {1, 2}, " abc\n  123\n  xyz" },
        { "abc\n123\nxyz\n", {1, 2}, " abc\n  123\n  xyz\n" },
        { "abc\n123\nxyz\n ", {1, 2}, " abc\n  123\n  xyz\n   " }
      };

      for (auto& s : tsli) {
        std::string ss(std::get<0>(s));
        common::Indent(ss, std::get<1>(s));
        if (ss != std::get<2>(s)) {
          Fail("unexpected indent (" + common::Join(std::get<1>(s), ", ") + ") of\n" +
               std::get<0>(s) + "\ngot:\n" + ss + "\nexpected:\n" + std::get<2>(s) + "\n");
        }
      }
    }

  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << std::endl;
    exit(1);
  }

  return 0;
}
