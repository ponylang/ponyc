#include <gtest/gtest.h>
#include <platform.h>

#include <pass/timing.h>

#include "util.h"

#include <chrono>
#include <string>
#include <thread>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <clocale>
#include <cctype>
#include <cmath>

// Read a whole file into a string (empty string if it can't be opened).
static std::string slurp(const char* path)
{
  FILE* f = fopen(path, "rb");
  if(f == NULL)
    return std::string();

  std::string out;
  char buf[4096];
  size_t n;
  while((n = fread(buf, 1, sizeof(buf), f)) > 0)
    out.append(buf, n);

  fclose(f);
  return out;
}


// A path in the temp dir unique to the calling test within one process.
static std::string temp_path(const char* tag)
{
  const testing::TestInfo* info =
    testing::UnitTest::GetInstance()->current_test_info();

  return std::string(testing::TempDir()) + "pony_timing_" +
    ((info != NULL) ? info->name() : "unknown") + "_" + tag + ".json";
}


// True iff `s` matches ^[0-9]+\.[0-9]{6}$ -- the exact shape json_print_seconds
// emits: one or more integer digits, a '.', then exactly six fractional digits.
static bool well_formed_seconds(const char* s)
{
  const char* p = s;
  if(!isdigit((unsigned char)*p))
    return false;
  while(isdigit((unsigned char)*p))
    p++;

  if(*p++ != '.')
    return false;

  for(int i = 0; i < 6; i++)
    if(!isdigit((unsigned char)*p++))
      return false;

  return *p == '\0';
}


// A minimal JSON validator: enough to reject the malformed output a mutation to
// the writer produces (a dangling or missing separator, an unterminated string,
// an unbalanced bracket, a bare or comma-decimal number).
namespace
{
  class JsonCheck
  {
  public:
    explicit JsonCheck(const std::string& s): m_s(s), m_i(0) {}

    bool valid()
    {
      ws();
      if(!value())
        return false;
      ws();
      return m_i == m_s.size();
    }

  private:
    bool at_end() { return m_i >= m_s.size(); }
    char peek() { return at_end() ? '\0' : m_s[m_i]; }

    void ws()
    {
      while(!at_end() && (isspace((unsigned char)m_s[m_i]) != 0))
        m_i++;
    }

    bool lit(const char* t)
    {
      size_t n = strlen(t);
      if(m_s.compare(m_i, n, t) != 0)
        return false;
      m_i += n;
      return true;
    }

    bool string()
    {
      if(peek() != '"')
        return false;
      m_i++;

      while(!at_end())
      {
        char c = m_s[m_i++];

        if(c == '"')
          return true;

        if(c == '\\')
        {
          if(at_end())
            return false;

          char e = m_s[m_i++];

          if(e == 'u')
          {
            for(int k = 0; k < 4; k++)
            {
              if(at_end() || (isxdigit((unsigned char)m_s[m_i]) == 0))
                return false;
              m_i++;
            }
          } else if(strchr("\"\\/bfnrt", e) == NULL) {
            return false;
          }
        } else if((unsigned char)c < 0x20) {
          // A raw control byte is not legal inside a JSON string.
          return false;
        }
      }

      return false;
    }

    bool number()
    {
      size_t start = m_i;

      if(peek() == '-')
        m_i++;

      if(at_end() || (isdigit((unsigned char)m_s[m_i]) == 0))
        return false;

      while(!at_end() && (isdigit((unsigned char)m_s[m_i]) != 0))
        m_i++;

      if(peek() == '.')
      {
        m_i++;
        if(at_end() || (isdigit((unsigned char)m_s[m_i]) == 0))
          return false;
        while(!at_end() && (isdigit((unsigned char)m_s[m_i]) != 0))
          m_i++;
      }

      return m_i > start;
    }

    bool array()
    {
      m_i++; // '['
      ws();

      if(peek() == ']')
      {
        m_i++;
        return true;
      }

      for(;;)
      {
        ws();
        if(!value())
          return false;
        ws();

        if(peek() == ',')
        {
          m_i++;
          continue;
        }

        if(peek() == ']')
        {
          m_i++;
          return true;
        }

        return false;
      }
    }

    bool object()
    {
      m_i++; // '{'
      ws();

      if(peek() == '}')
      {
        m_i++;
        return true;
      }

      for(;;)
      {
        ws();
        if(!string())
          return false;
        ws();
        if(peek() != ':')
          return false;
        m_i++;
        ws();
        if(!value())
          return false;
        ws();

        if(peek() == ',')
        {
          m_i++;
          continue;
        }

        if(peek() == '}')
        {
          m_i++;
          return true;
        }

        return false;
      }
    }

    bool value()
    {
      switch(peek())
      {
        case '{': return object();
        case '[': return array();
        case '"': return string();
        case 't': return lit("true");
        case 'f': return lit("false");
        case 'n': return lit("null");
        default:  return number();
      }
    }

    const std::string& m_s;
    size_t m_i;
  };
}

static bool is_valid_json(const std::string& s)
{
  JsonCheck c(s);
  return c.valid();
}


class TimingTest: public testing::Test {};


// Every entry point must tolerate a NULL context -- the timing-off default,
// which the instrumentation call sites pass unguarded.
TEST_F(TimingTest, NullContextIsNoOp)
{
  pass_timers_free(NULL);
  pass_timers_enable_table(NULL);
  EXPECT_TRUE(pass_timers_set_json(NULL, "unused.json"));
  pass_timers_set_report_meta(NULL, true, "triple");
  pass_timers_start(NULL, "pkg", "expr");
  pass_timers_stop(NULL, "pkg", "expr");
  EXPECT_EQ(0u, pass_timers_depth(NULL, "pkg", "expr"));
  EXPECT_EQ(0.0, pass_timers_wall(NULL, "pkg", "expr"));
  EXPECT_TRUE(pass_timers_report(NULL));
}


// Depth alone would still pass with the stop-side guard dropped, so this
// test also asserts the recorded wall time. The timer banks wall time only
// in stopTimer, so the region must read zero until the outer stop and
// non-zero after it.
TEST_F(TimingTest, ReentrantSameRegionCountsSpanOnce)
{
  pass_timers_t* t = pass_timers_create();

  pass_timers_start(t, "builtin", "expr");
  EXPECT_EQ(1u, pass_timers_depth(t, "builtin", "expr"));

  pass_timers_start(t, "builtin", "expr");                 // re-enter
  EXPECT_EQ(2u, pass_timers_depth(t, "builtin", "expr"));

  // An empty span can measure zero. Sleeping here widens both the inner span
  // and the outer span that contains it.
  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  pass_timers_stop(t, "builtin", "expr");                  // inner stop
  EXPECT_EQ(1u, pass_timers_depth(t, "builtin", "expr"));
  // Still running: nothing has been banked. A missing guard on the stop side
  // would have banked the inner span here.
  EXPECT_EQ(0.0, pass_timers_wall(t, "builtin", "expr"));

  pass_timers_stop(t, "builtin", "expr");                  // outer stop
  EXPECT_EQ(0u, pass_timers_depth(t, "builtin", "expr"));
  EXPECT_GT(pass_timers_wall(t, "builtin", "expr"), 0.0);

  pass_timers_free(t);
}


// A stop with no matching start must be ignored and must leave no row: the
// stderr table skips a timer that never ran, so a row created here would appear
// in the JSON and nowhere else. The depth assertion fails on the unsigned
// underflow that dropping the guard produces -- `--depth == 0` on a zero
// unsigned is false, so the timer is never touched and a wall-time assertion
// alone would still pass.
TEST_F(TimingTest, UnbalancedStopIsIgnoredAndCreatesNoRow)
{
  std::string path = temp_path("rows");

  pass_timers_t* t = pass_timers_create();
  ASSERT_TRUE(pass_timers_set_json(t, path.c_str()));

  pass_timers_stop(t, "builtin", "never_started");

  EXPECT_EQ(0u, pass_timers_depth(t, "builtin", "never_started"));
  EXPECT_EQ(0.0, pass_timers_wall(t, "builtin", "never_started"));

  EXPECT_TRUE(pass_timers_report(t));
  pass_timers_free(t);

  std::string json = slurp(path.c_str());
  remove(path.c_str());

  ASSERT_FALSE(json.empty());
  EXPECT_TRUE(is_valid_json(json)) << json;
  EXPECT_EQ(std::string::npos, json.find("never_started"));
  // The array is empty, not merely missing the phantom row.
  EXPECT_EQ(std::string::npos, json.find("\"package\":"));
}


// Every byte must round-trip through the escaper as exactly one known form.
// Sampling a few bytes is not enough: with the sampled bytes adjacent, one
// escape can stand in for another and a dropped branch still satisfies a
// substring search.
TEST_F(TimingTest, EveryByteIsEscapedExactly)
{
  for(int b = 1; b < 256; b++) // 0 terminates a C string, so it cannot appear
  {
    std::string path = temp_path("byte");

    // Surround the byte so its escape cannot borrow a neighbour's.
    char pkg[4] = {'a', (char)b, 'z', '\0'};

    pass_timers_t* t = pass_timers_create();
    ASSERT_TRUE(pass_timers_set_json(t, path.c_str()));
    pass_timers_start(t, pkg, "expr");
    pass_timers_stop(t, pkg, "expr");
    ASSERT_TRUE(pass_timers_report(t));
    pass_timers_free(t);

    std::string json = slurp(path.c_str());
    remove(path.c_str());

    ASSERT_FALSE(json.empty()) << "byte " << b;
    ASSERT_TRUE(is_valid_json(json)) << "byte " << b << ": " << json;

    // What the writer must have produced for this byte.
    std::string want;

    switch(b)
    {
      case '"':  want = "\\\""; break;
      case '\\': want = "\\\\"; break;
      case '\b': want = "\\b"; break;
      case '\f': want = "\\f"; break;
      case '\n': want = "\\n"; break;
      case '\r': want = "\\r"; break;
      case '\t': want = "\\t"; break;
      default:
        if(b < 0x20)
        {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04x", b);
          want = buf;
        } else if(b < 0x80) {
          want = std::string(1, (char)b);
        } else {
          // A lone byte >= 0x80 is never well-formed UTF-8 on its own, so it
          // must be replaced rather than passed through raw -- a raw one makes
          // the file invalid UTF-8, which a strict parser rejects.
          want = "\xEF\xBF\xBD";
        }
    }

    std::string expected = std::string("\"package\": \"a") + want + "z\"";
    EXPECT_NE(std::string::npos, json.find(expected))
      << "byte " << b << " not escaped as expected in: " << json;
  }
}


// Well-formed multi-byte UTF-8 must survive verbatim: replacing every byte
// >= 0x80 would keep the file valid while mangling legitimate package names.
TEST_F(TimingTest, WellFormedUtf8PassesThrough)
{
  std::string path = temp_path("utf8");

  const char* pkg = "caf\xC3\xA9/\xE2\x9C\x93/\xF0\x9F\x8E\x89";

  pass_timers_t* t = pass_timers_create();
  ASSERT_TRUE(pass_timers_set_json(t, path.c_str()));
  pass_timers_start(t, pkg, "expr");
  pass_timers_stop(t, pkg, "expr");
  ASSERT_TRUE(pass_timers_report(t));
  pass_timers_free(t);

  std::string json = slurp(path.c_str());
  remove(path.c_str());

  ASSERT_FALSE(json.empty());
  EXPECT_TRUE(is_valid_json(json)) << json;
  EXPECT_NE(std::string::npos,
    json.find(std::string("\"package\": \"") + pkg + "\""));
}


// A truncated multi-byte sequence, an overlong encoding and a surrogate half
// are all ill-formed and must each become U+FFFD rather than reaching the file.
TEST_F(TimingTest, IllFormedUtf8BecomesReplacementChar)
{
  const char* cases[] =
  {
    "a\xC3z",         // truncated 2-byte sequence
    "a\xE2\x9Cz",     // truncated 3-byte sequence
    "a\xC0\xAFz",     // overlong encoding of '/'
    "a\xED\xA0\x80z", // UTF-16 surrogate half
    "a\xF5\x80\x80\x80z" // beyond U+10FFFF
  };

  for(const char* pkg : cases)
  {
    std::string path = temp_path("illformed");

    pass_timers_t* t = pass_timers_create();
    ASSERT_TRUE(pass_timers_set_json(t, path.c_str()));
    pass_timers_start(t, pkg, "expr");
    pass_timers_stop(t, pkg, "expr");
    ASSERT_TRUE(pass_timers_report(t));
    pass_timers_free(t);

    std::string json = slurp(path.c_str());
    remove(path.c_str());

    ASSERT_FALSE(json.empty()) << pkg;
    EXPECT_TRUE(is_valid_json(json)) << pkg << ": " << json;
    EXPECT_NE(std::string::npos, json.find("\xEF\xBF\xBD")) << pkg;
  }
}


// Two regions exercise the row separator, which one region cannot: a writer
// that never advances past its first row emits a leading comma, and the file
// stops being JSON. Row order is by name; a consumer relies on that being
// stable across runs.
TEST_F(TimingTest, MultipleRowsAreSeparatedAndNameOrdered)
{
  std::string path = temp_path("rows2");

  pass_timers_t* t = pass_timers_create();
  ASSERT_TRUE(pass_timers_set_json(t, path.c_str()));

  // Registered in the opposite order to the one they must come out in.
  pass_timers_start(t, "zzz", "expr");
  pass_timers_stop(t, "zzz", "expr");
  pass_timers_start(t, "aaa", "expr");
  pass_timers_stop(t, "aaa", "expr");

  ASSERT_TRUE(pass_timers_report(t));
  pass_timers_free(t);

  std::string json = slurp(path.c_str());
  remove(path.c_str());

  ASSERT_FALSE(json.empty());
  EXPECT_TRUE(is_valid_json(json)) << json;

  size_t a = json.find("\"package\": \"aaa\"");
  size_t z = json.find("\"package\": \"zzz\"");
  ASSERT_NE(std::string::npos, a);
  ASSERT_NE(std::string::npos, z);
  EXPECT_LT(a, z);
}


// Regions accumulate by (package, pass) across start/stop pairs. This is what
// lets module_passes time parse per module and still report one row per
// package, so it needs a test of its own.
TEST_F(TimingTest, RepeatedRegionAccumulatesIntoOneRow)
{
  std::string path = temp_path("accum");

  pass_timers_t* t = pass_timers_create();
  ASSERT_TRUE(pass_timers_set_json(t, path.c_str()));

  pass_timers_start(t, "builtin", "parse");
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  pass_timers_stop(t, "builtin", "parse");
  double first = pass_timers_wall(t, "builtin", "parse");

  pass_timers_start(t, "builtin", "parse");
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  pass_timers_stop(t, "builtin", "parse");
  double second = pass_timers_wall(t, "builtin", "parse");

  EXPECT_GT(second, first);

  ASSERT_TRUE(pass_timers_report(t));
  pass_timers_free(t);

  std::string json = slurp(path.c_str());
  remove(path.c_str());

  ASSERT_FALSE(json.empty());
  EXPECT_TRUE(is_valid_json(json)) << json;

  // One row, not two.
  size_t first_row = json.find("\"package\": \"builtin\"");
  ASSERT_NE(std::string::npos, first_row);
  EXPECT_EQ(std::string::npos,
    json.find("\"package\": \"builtin\"", first_row + 1));
}


// A context configured for JSON only still writes its file: the JSON output
// does not depend on the stderr table.
TEST_F(TimingTest, JsonOnlyWritesFileAndPrintsNothing)
{
  std::string path = temp_path("jsononly");

  testing::internal::CaptureStderr();

  pass_timers_t* t = pass_timers_create();
  ASSERT_TRUE(pass_timers_set_json(t, path.c_str()));
  pass_timers_start(t, "builtin", "reach");
  pass_timers_stop(t, "builtin", "reach");
  ASSERT_TRUE(pass_timers_report(t));
  pass_timers_free(t);

  std::string err = testing::internal::GetCapturedStderr();

  std::string json = slurp(path.c_str());
  remove(path.c_str());

  ASSERT_FALSE(json.empty());
  EXPECT_TRUE(is_valid_json(json)) << json;
  EXPECT_NE(std::string::npos, json.find("\"pass\": \"reach\""));
  EXPECT_EQ("", err);
}


// The stderr table must carry the row, the elapsed-wall denominator (without
// which the reader cannot tell what share of the build the rows cover), and the
// statement that only the front end is timed.
TEST_F(TimingTest, TablePrintsRowsAndDenominator)
{
  testing::internal::CaptureStderr();

  pass_timers_t* t = pass_timers_create();
  pass_timers_enable_table(t);
  pass_timers_start(t, "mypkg", "expr");
  pass_timers_stop(t, "mypkg", "expr");
  ASSERT_TRUE(pass_timers_report(t));
  pass_timers_free(t);

  std::string err = testing::internal::GetCapturedStderr();

  EXPECT_NE(std::string::npos, err.find("mypkg (expr)")) << err;
  EXPECT_NE(std::string::npos, err.find("elapsed")) << err;
  EXPECT_NE(std::string::npos, err.find("front-end")) << err;
}


// The nesting caveat describes an effect that did not occur when no two regions
// overlapped, so it must not be printed then.
TEST_F(TimingTest, TableOmitsNestingNoteWhenNothingNested)
{
  testing::internal::CaptureStderr();

  pass_timers_t* t = pass_timers_create();
  pass_timers_enable_table(t);
  pass_timers_start(t, "a", "parse");
  pass_timers_stop(t, "a", "parse");
  pass_timers_start(t, "b", "parse");
  pass_timers_stop(t, "b", "parse");
  ASSERT_TRUE(pass_timers_report(t));
  pass_timers_free(t);

  std::string err = testing::internal::GetCapturedStderr();

  EXPECT_EQ(std::string::npos, err.find("nest")) << err;
}


TEST_F(TimingTest, TableIncludesNestingNoteWhenRegionsNested)
{
  testing::internal::CaptureStderr();

  pass_timers_t* t = pass_timers_create();
  pass_timers_enable_table(t);
  pass_timers_start(t, "outer", "scope");
  pass_timers_start(t, "inner", "parse");
  pass_timers_stop(t, "inner", "parse");
  pass_timers_stop(t, "outer", "scope");
  ASSERT_TRUE(pass_timers_report(t));
  pass_timers_free(t);

  std::string err = testing::internal::GetCapturedStderr();

  EXPECT_NE(std::string::npos, err.find("nest")) << err;
}


// A context that collected nothing must say so rather than print nothing: total
// silence is indistinguishable from the option not being recognised.
TEST_F(TimingTest, TableWithNoRegionsSaysSo)
{
  testing::internal::CaptureStderr();

  pass_timers_t* t = pass_timers_create();
  pass_timers_enable_table(t);
  ASSERT_TRUE(pass_timers_report(t));
  pass_timers_free(t);

  std::string err = testing::internal::GetCapturedStderr();

  EXPECT_NE(std::string::npos, err.find("No pass timings collected")) << err;
}


// An unwritable path must be refused when it is set, not after a whole build
// has run, and must be reported as a failure rather than silently dropped.
TEST_F(TimingTest, UnwritablePathIsRefusedUpFront)
{
  testing::internal::CaptureStderr();

  pass_timers_t* t = pass_timers_create();
  bool ok = pass_timers_set_json(t,
    "no_such_directory_here/nested/timings.json");
  pass_timers_free(t);

  std::string err = testing::internal::GetCapturedStderr();

  EXPECT_FALSE(ok);
  EXPECT_NE(std::string::npos, err.find("cannot write")) << err;
}


// The report is built beside the target and renamed over it, so a run that
// cannot finish leaves the previous file untouched rather than truncated. A
// consumer that reads the file after a failed run must not get half of one.
TEST_F(TimingTest, FailedRunLeavesPreviousFileIntact)
{
  std::string path = temp_path("intact");

  // An earlier run's file.
  FILE* f = fopen(path.c_str(), "wb");
  ASSERT_NE((FILE*)NULL, f);
  fputs("{\"previous\": true}\n", f);
  fclose(f);

  pass_timers_t* t = pass_timers_create();
  ASSERT_TRUE(pass_timers_set_json(t, path.c_str()));
  pass_timers_start(t, "builtin", "parse");
  pass_timers_stop(t, "builtin", "parse");

  // Freed without reporting, as a build killed part way through would.
  pass_timers_free(t);

  std::string json = slurp(path.c_str());
  remove(path.c_str());

  EXPECT_EQ("{\"previous\": true}\n", json);
  // The temp file is not left behind either.
  EXPECT_TRUE(slurp((path + ".tmp").c_str()).empty());
}


// pass_timers_set_report_meta must surface the build status and target triple
// as top-level fields, alongside the compiler version, so a stored file
// describes the build it came from.
TEST_F(TimingTest, MetaFieldsAppearInJson)
{
  std::string path = temp_path("meta");

  pass_timers_t* t = pass_timers_create();
  ASSERT_TRUE(pass_timers_set_json(t, path.c_str()));
  pass_timers_set_report_meta(t, false, "x86_64-unknown-linux-gnu");
  pass_timers_start(t, "builtin", "parse");
  pass_timers_stop(t, "builtin", "parse");
  ASSERT_TRUE(pass_timers_report(t));
  pass_timers_free(t);

  std::string json = slurp(path.c_str());
  remove(path.c_str());

  ASSERT_FALSE(json.empty());
  EXPECT_TRUE(is_valid_json(json)) << json;
  EXPECT_NE(std::string::npos, json.find("\"status\": \"failed\""));
  // Present and non-empty: dropping the value would still leave the key.
  size_t ver = json.find("\"compiler_version\": \"");
  ASSERT_NE(std::string::npos, ver);
  EXPECT_NE('"', json[ver + strlen("\"compiler_version\": \"")]);
  EXPECT_NE(std::string::npos,
    json.find("\"triple\": \"x86_64-unknown-linux-gnu\""));
  EXPECT_NE(std::string::npos, json.find("\"elapsed_wall\""));
}


// A NULL triple omits the field rather than writing it empty. main.c passes
// NULL when the build failed before the target was resolved.
TEST_F(TimingTest, NullTripleOmitsTheField)
{
  std::string path = temp_path("nulltriple");

  pass_timers_t* t = pass_timers_create();
  ASSERT_TRUE(pass_timers_set_json(t, path.c_str()));
  pass_timers_set_report_meta(t, true, NULL);
  pass_timers_start(t, "builtin", "parse");
  pass_timers_stop(t, "builtin", "parse");
  ASSERT_TRUE(pass_timers_report(t));
  pass_timers_free(t);

  std::string json = slurp(path.c_str());
  remove(path.c_str());

  ASSERT_FALSE(json.empty());
  EXPECT_TRUE(is_valid_json(json)) << json;
  EXPECT_NE(std::string::npos, json.find("\"status\": \"ok\""));
  EXPECT_EQ(std::string::npos, json.find("\"triple\""));
}


// With no metadata set at all, status defaults to "ok" and no triple is
// written.
TEST_F(TimingTest, DefaultMetaIsOkAndOmitsTriple)
{
  std::string path = temp_path("defmeta");

  pass_timers_t* t = pass_timers_create();
  ASSERT_TRUE(pass_timers_set_json(t, path.c_str()));
  pass_timers_start(t, "builtin", "parse");
  pass_timers_stop(t, "builtin", "parse");
  ASSERT_TRUE(pass_timers_report(t));
  pass_timers_free(t);

  std::string json = slurp(path.c_str());
  remove(path.c_str());

  ASSERT_FALSE(json.empty());
  EXPECT_TRUE(is_valid_json(json)) << json;
  EXPECT_NE(std::string::npos, json.find("\"status\": \"ok\""));
  EXPECT_EQ(std::string::npos, json.find("\"triple\""));
}


// JSON numbers must use '.' regardless of the process LC_NUMERIC: %f honours a
// comma-decimal locale and would emit invalid JSON, the integer formatting in
// json_print_seconds does not. Catching the %f regression requires actually
// being in a comma-decimal locale, so this skips rather than false-passing
// where the platform has none: in a "C"-only environment even %f prints '.',
// so a passing assertion there would prove nothing.
TEST_F(TimingTest, JsonNumbersAreLocaleIndependent)
{
  const char* comma_locales[] =
    {"de_DE.UTF-8", "de_DE", "fr_FR.UTF-8", "fr_FR", "nl_NL.UTF-8", "nl_NL"};
  bool have_comma = false;
  for(const char* loc : comma_locales)
  {
    if((setlocale(LC_NUMERIC, loc) != NULL) &&
      (localeconv()->decimal_point[0] == ','))
    {
      have_comma = true;
      break;
    }
  }

  if(!have_comma)
  {
    setlocale(LC_NUMERIC, "C");
    GTEST_SKIP() << "no comma-decimal locale installed; cannot exercise the "
      "locale-dependent formatting path";
  }

  std::string path = temp_path("locale");

  pass_timers_t* t = pass_timers_create();
  ASSERT_TRUE(pass_timers_set_json(t, path.c_str()));
  pass_timers_start(t, "builtin", "parse");
  pass_timers_stop(t, "builtin", "parse");
  ASSERT_TRUE(pass_timers_report(t));
  pass_timers_free(t);

  setlocale(LC_NUMERIC, "C"); // restore before other tests run

  std::string json = slurp(path.c_str());
  remove(path.c_str());

  ASSERT_FALSE(json.empty());
  // The validator rejects a comma decimal separator outright, since a number
  // must end where the comma starts and what follows is not a valid member.
  EXPECT_TRUE(is_valid_json(json)) << json;
}


// The emitted number format -- integer digits, '.', six fractional digits -- is
// a fixed contract, and the value must be right, not merely well shaped: a
// wrong scale factor still produces a well-formed string.
TEST_F(TimingTest, SecondsFormatIsExact)
{
  setlocale(LC_NUMERIC, "C");

  struct { double v; const char* want; } cases[] =
  {
    {0.0,        "0.000000"},
    {0.5,        "0.500000"},
    {1.0,        "1.000000"},
    {9.999999,   "9.999999"},
    {123.456789, "123.456789"},
    {1000000.0,  "1000000.000000"}
  };

  char buf[32];

  for(auto& c : cases)
  {
    pass_timers_format_seconds(c.v, buf, sizeof(buf));
    EXPECT_STREQ(c.want, buf);
    EXPECT_TRUE(well_formed_seconds(buf)) << "not well-formed: " << buf;
  }
}


// The two branches the timer's own values rarely reach: a negative duration
// clamps to zero rather than emitting a '-' (which LLVM's realtime wall clock
// can produce if the system clock steps back), and a fractional part that
// rounds up to a whole second carries instead of printing ".1000000".
TEST_F(TimingTest, SecondsFormatClampsAndCarries)
{
  char buf[32];

  pass_timers_format_seconds(-1.5, buf, sizeof(buf));
  EXPECT_STREQ("0.000000", buf); // clamp: negative -> 0

  pass_timers_format_seconds(0.9999999, buf, sizeof(buf));
  EXPECT_STREQ("1.000000", buf); // carry: 999999.9 us rounds to a full second
}


// NaN and values past the integer conversion's range must not reach that
// conversion, where the behaviour is undefined and the output is not a number.
TEST_F(TimingTest, SecondsFormatRejectsNanAndOutOfRange)
{
  char buf[32];

  pass_timers_format_seconds(NAN, buf, sizeof(buf));
  EXPECT_TRUE(well_formed_seconds(buf)) << buf;

  pass_timers_format_seconds(INFINITY, buf, sizeof(buf));
  EXPECT_TRUE(well_formed_seconds(buf)) << buf;

  pass_timers_format_seconds(1e19, buf, sizeof(buf));
  EXPECT_TRUE(well_formed_seconds(buf)) << buf;

  pass_timers_format_seconds(-INFINITY, buf, sizeof(buf));
  EXPECT_STREQ("0.000000", buf);
}


// The instrumentation in pass.c is what the option actually drives. A compile
// must leave every region it opened closed -- a start whose stop is missed on
// some return path leaves the timer running, and the row then reports 0.000000
// next to the pass that is actually slow.
class TimingPassTest: public PassTest {};


TEST_F(TimingPassTest, CompileLeavesNoRegionRunning)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  opt.timers = pass_timers_create();

  DO(test_compile(src, "expr"));

  static const char* passes[] =
    {"parse", "syntax", "sugar", "scope", "refer", "expr"};

  for(const char* p : passes)
    EXPECT_EQ(0u, pass_timers_depth(opt.timers, "prog", p)) << p;

  // The passes really ran on the package, so the assertion above is not
  // vacuously true of regions that were never opened.
  EXPECT_GT(pass_timers_wall(opt.timers, "prog", "expr"), 0.0);

  // TearDown's pass_opt_done frees the context.
}


// The same must hold when a pass fails: the error paths out of ast_visit are
// the ones a missing stop hides in.
TEST_F(TimingPassTest, FailedCompileLeavesNoRegionRunning)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U8 = \"not a number\"";

  opt.timers = pass_timers_create();

  DO(test_error(src, "expr"));

  static const char* passes[] =
    {"parse", "syntax", "sugar", "scope", "refer", "expr"};

  for(const char* p : passes)
    EXPECT_EQ(0u, pass_timers_depth(opt.timers, "prog", p)) << p;
}


// The finaliser pass does not run through ast_visit, so it is timed at its own
// call site.
TEST_F(TimingPassTest, FinaliserPassIsTimed)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None\n"
    "  fun _final() =>\n"
    "    None";

  opt.timers = pass_timers_create();

  DO(test_compile(src, "final"));

  EXPECT_EQ(0u, pass_timers_depth(opt.timers, "prog", "final"));
  EXPECT_GT(pass_timers_wall(opt.timers, "prog", "final"), 0.0);
}
