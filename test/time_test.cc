// Copyright 2023, David Goodwin. All rights reserved.

#include <iostream>
#include <string>
#include <vector>
#include "common_error.h"
#include "common_time.h"

namespace {

#define Fail(MSG)                                                     \
  do {                                                                \
    std::cerr << "error: " << (MSG) << std::endl;                     \
    exit(1);                                                          \
  } while (false)

} // namespace

// time_test
int
main(int argc, char **argv)
{
  // Echo command line
  for (auto i = 0; i < argc; ++i) {
    std::cout << argv[i] << " ";
  }
  std::cout << std::endl;

  try {

    // Test equivalent values.
    {
      const std::string ms72_str("7.2ms");
      common::Time ms72(ms72_str);

      std::vector<std::string> tcs {
        " 7.2ms", "7.2 ms", "7.2ms ", "  7.2   ms   ",
        "07.2ms", "007.200ms", "7.2ms", "7.2 ms", "7 .2 ms",
        "7200us", "07200 us",
        ".0072s", "0.0072s", "0. 0072 s",
        ".00012m", "0.000120m",
        ".000002h"
      };

      for (auto& tc : tcs) {
        common::Time tct(tc);
        if (tct != ms72) {
          Fail("unexpected '" + ms72_str + "' != '" + tc + "'");
        }
      }
    }

    // Test valid
    {
      const std::string ms42_str("42ms");
      common::Time ms42(ms42_str);

      std::vector<std::string> tcs {
        " 42ms", "42 ms", "42ms ", "  42   ms   ",
        "042ms", "0042.00ms", "42.ms", "42. ms", "42 . ms",
        "42 .ms", "42 .0ms", "42.0 ms", "42.0ms",
        "42000us", "42000 us",
        ".042s", "0.042s", "0. 042 s"
      };

      for (auto& tc : tcs) {
        common::Time tct(tc);
        if (tct != ms42) {
          Fail("unexpected '" + ms42_str + "' != '" + tc + "'");
        }
      }
    }

    // Test expected contructor fail.
    {
      const std::string us42_str("42us");
      common::Time us42(us42_str);

      std::vector<std::string> tcs {
        " 42u s"
      };

      for (auto& tc : tcs) {
        try {
          common::Time tct(tc);
          Fail("unexpected pass '" + tc + "' constructed");
        } catch (common::Exception& ex) {
          // nothing
        }
      }
    }

    // Test expected fail parsing.
    {
      const std::string s4212_str("42.12s");
      common::Time s4212(s4212_str);

      std::vector<std::string> tcs {
        "4 2 .12s", "42. 1 2 s"
      };

      for (auto& tc : tcs) {
        common::Time tct(tc);
        if (tct == s4212) {
          Fail("unexpected pass '" + s4212_str + "' == '" + tc + "'");
        }
      }

      std::vector<std::string> tcs2 {
        "42.1200000s"
      };

      for (auto& tc : tcs2) {
        try {
          common::Time tct(tc);
          Fail("unexpected pass '" + tc + "' constructed");
        } catch (common::Exception& ex) {
          // nothing
        }
      }
    }

    // Test operators
    {
      const std::string a_str("1.1s");
      common::Time a(a_str);

      const std::string b_str("42.42ms");
      common::Time b(b_str);

      const std::string add_str("1.14242s");
      common::Time add(add_str);
      if ((a + b) != add) {
        Fail("unexpected '" + a_str + "' + '" + b_str + "' != '" + add_str + "'");
      }

      common::Time c = a;
      c += b;
      if (c != add) {
        Fail("unexpected '" + a_str + "' += '" + b_str + "' != '" + add_str + "'");
      }

      const std::string sub_str("1.05758s");
      common::Time sub(sub_str);
      if ((a - b) != sub) {
        Fail("unexpected '" + a_str + "' - '" + b_str + "' != '" + sub_str + "'");
      }

      common::Time d = a;
      d -= b;
      if (d != sub) {
        Fail("unexpected '" + a_str + "' -= '" + b_str + "' != '" + sub_str + "'");
      }
    }

    // * and *=
    {
      const std::string m_str("123ms");
      common::Time m(m_str);
      if ((m * 0) != common::Time()) {
        Fail("unexpected '" + m_str + "' * 0 != 0");
      }
      m *= 0;
      if (m != common::Time()) {
        Fail("unexpected '" + m_str + "' *= 0 != 0");
      }

      m = m_str;
      if ((m * 1) != common::Time(m_str)) {
        Fail("unexpected '" + m_str + "' * 1 != '123ms'");
      }
      m *= 1;
      if (m != common::Time(m_str)) {
        Fail("unexpected '" + m_str + "' *= 1 != '123ms'");
      }

      m = m_str;
      if ((m * 3) != common::Time("369ms")) {
        Fail("unexpected '" + m_str + "' * 3 != '369ms'");
      }
      m *= 3;
      if (m != common::Time("369ms")) {
        Fail("unexpected '" + m_str + "' *= 3 != '369ms'");
      }
    }

    // / and /=
    {
      const std::string m_str("123ms");
      common::Time m(m_str);
      if ((m / 1) != common::Time(m_str)) {
        Fail("unexpected '" + m_str + "' / 1 != '123ms'");
      }
      m /= 1;
      if (m != common::Time(m_str)) {
        Fail("unexpected '" + m_str + "' /= 1 != '123ms'");
      }

      m = m_str;
      if ((m / 2) != common::Time("61.5ms")) {
        Fail("unexpected '" + m_str + "' / 2 != '61.5ms'");
      }
      m /= 2;
      if (m != common::Time("61.5ms")) {
        Fail("unexpected '" + m_str + "' /= 2 != '61.5ms'");
      }
    }

    // % and %=
    {
      const std::string m_str("123us");
      common::Time m(m_str);
      if ((m % 1) != common::Time()) {
        Fail("unexpected '" + m_str + "' % 1 != '0us'");
      }
      m %= 1;
      if (m != common::Time()) {
        Fail("unexpected '" + m_str + "' %= 1 != '0us'");
      }

      m = m_str;
      if ((m % 20) != common::Time("3us")) {
        Fail("unexpected '" + m_str + "' % 20 != '3us'");
      }
      m %= 20;
      if (m != common::Time("3us")) {
        Fail("unexpected '" + m_str + "' %= 20 != '3us'");
      }
    }

    // Count
    {
      const std::string c_str("1.123h");
      common::Time c(c_str);
      if (c.Count(common::Time::TimeUnit::HOUR) != 1) {
        Fail("unexpected '" + c_str + "' Count(HOUR) != 1");
      }
      if (c.Count(common::Time::TimeUnit::MINUTE) != 67) {
        Fail("unexpected '" + c_str + "' Count(MINUTE) != 67");
      }
      if (c.Count(common::Time::TimeUnit::SECOND) != 4042) {
        Fail("unexpected '" + c_str + "' Count(SECOND) != 4042");
      }
      if (c.Count(common::Time::TimeUnit::MILLISECOND) != 4042800) {
        Fail("unexpected '" + c_str + "' Count(MILLISECOND) != 4042800");
      }
      if (c.Count(common::Time::TimeUnit::MICROSECOND) != 4042800000) {
        Fail("unexpected '" + c_str + "' Count(MICROSECOND) != 4042800000");
      }
    }

    // NDeltas
    {
      const std::string m_str("1m");
      common::Time m(m_str);
      if ((m.NDeltas(common::Time("1s")) != 60) || (m != common::Time("0s"))) {
        Fail("unexpected '" + m_str + "' NDeltas '" + "1s" + "' != 60");
      }

      m = m_str;
      if ((m.NDeltas(common::Time("1h")) != 0) || (m != common::Time("1m"))) {
        Fail("unexpected '" + m_str + "' NDeltas '" + "1h" + "' != 0");
      }

      m = m_str;
      if ((m.NDeltas(common::Time("29.999s"), common::Time("3ms")) != 2) ||
          (m != common::Time("5ms"))) {
        Fail("unexpected '" + m_str + "' NDeltas '" + "29.999s, 3ms" + "' != 2");
      }

      const std::string ms_str("5.2ms");
      common::Time ms(ms_str);
      if ((ms.NDeltas(common::Time("1ms")) != 5) || (ms != common::Time("200us"))) {
        Fail("unexpected '" + ms_str + "' NDeltas '" + "1ms" + "' != 5");
      }

      ms = ms_str;
      if ((ms.NDeltas(common::Time("5ms"), common::Time("5us")) != 1) ||
          (ms != common::Time("205us"))) {
        Fail("unexpected '" + ms_str + "' NDeltas '" + "5ms, 5us" + "' != 1");
      }

      ms = ms_str;
      if ((ms.NDeltas(common::Time("30us")) != 173) || (ms != common::Time("10us"))) {
        Fail("unexpected '" + ms_str + "' NDeltas '" + "30us" + "' != 173");
      }
    }

  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << std::endl;
    exit(1);
  }

  return 0;
}
