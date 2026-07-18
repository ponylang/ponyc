#include <gtest/gtest.h>
#include <platform.h>

#include <pass/timing.h>

#include <string>
#include <cstdio>
#include <clocale>
#include <cctype>

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


class TimingTest: public testing::Test {};


// Every entry point must tolerate a NULL context -- that is the timing-off
// default taken on every compile that lacks --pass-timings, and instrumentation
// call sites pass opt->timers unguarded. Removing any `t == nullptr` guard
// segfaults the default compile path right here.
TEST_F(TimingTest, NullContextIsNoOp)
{
  pony_timers_free(NULL);
  pony_timers_enable_stderr(NULL);
  pony_timers_set_json(NULL, "unused.json");
  pony_timer_start(NULL, "g", "n");
  pony_timer_stop(NULL, "g", "n");
  pony_timer_start_pair(NULL, "g", "a", "b");
  pony_timer_stop_pair(NULL, "g", "a", "b");
  pony_timers_report(NULL);
  SUCCEED();
}


// A region must nest inside itself (the expr pass re-enters expr on a
// generic instantiation). The depth guard collapses the nesting; without it the
// second startTimer() starts an already-running llvm::Timer -- an abort on an
// assertions-enabled LLVM ("Start/Stop out of order").
TEST_F(TimingTest, ReentrantSameRegionDoesNotAbort)
{
  pony_timers_t* t = pony_timers_create();
  pony_timer_start(t, "phases", "expr");
  pony_timer_start(t, "phases", "expr");
  pony_timer_stop(t, "phases", "expr");
  pony_timer_stop(t, "phases", "expr");
  pony_timers_free(t);
  SUCCEED();
}


// A stop with no matching start must be ignored, not turned into a stopTimer()
// on an already-stopped timer (an abort on an assertions-enabled LLVM).
TEST_F(TimingTest, UnbalancedStopIsIgnored)
{
  pony_timers_t* t = pony_timers_create();
  pony_timer_stop(t, "phases", "never_started");
  pony_timer_stop_pair(t, "package_passes", "pkg", "expr");
  pony_timers_free(t);
  SUCCEED();
}


// The JSON must escape control/quote/backslash bytes (a raw byte makes an
// invalid file a strict parser rejects) and must emit a pair region's two parts
// as separate fields, so a consumer need not split the composed display string.
TEST_F(TimingTest, JsonEscapesAndEmitsPairParts)
{
  std::string path = std::string(testing::TempDir()) + "pony_timing_pairs.json";

  pony_timers_t* t = pony_timers_create();
  pony_timers_set_json(t, path.c_str());

  pony_timer_start(t, "phases", "parse");
  pony_timer_stop(t, "phases", "parse");

  // Package part with a quote, a backslash and a control byte.
  const char* tricky = "pk\"g\\\x01";
  pony_timer_start_pair(t, "package_passes", tricky, "expr");
  pony_timer_stop_pair(t, "package_passes", tricky, "expr");

  pony_timers_report(t);
  pony_timers_free(t);

  std::string json = slurp(path.c_str());
  remove(path.c_str());

  ASSERT_FALSE(json.empty());
  EXPECT_NE(std::string::npos, json.find("\"phases\""));
  EXPECT_NE(std::string::npos, json.find("\"package_passes\""));
  // Pair parts as their own fields.
  EXPECT_NE(std::string::npos, json.find("\"name_a\""));
  EXPECT_NE(std::string::npos, json.find("\"name_b\": \"expr\""));
  // Escaping: quote -> \", backslash -> \\, control byte -> .
  EXPECT_NE(std::string::npos, json.find("\\\""));
  EXPECT_NE(std::string::npos, json.find("\\\\"));
  EXPECT_NE(std::string::npos, json.find("\\u0001"));
}


// A plain (non-pair) region must not carry name_a/name_b fields.
TEST_F(TimingTest, PlainRegionOmitsPairParts)
{
  std::string path = std::string(testing::TempDir()) + "pony_timing_plain.json";

  pony_timers_t* t = pony_timers_create();
  pony_timers_set_json(t, path.c_str());
  pony_timer_start(t, "phases", "parse");
  pony_timer_stop(t, "phases", "parse");
  pony_timers_report(t);
  pony_timers_free(t);

  std::string json = slurp(path.c_str());
  remove(path.c_str());

  ASSERT_FALSE(json.empty());
  EXPECT_EQ(std::string::npos, json.find("name_a"));
}


// A context for JSON only (no pony_timers_enable_stderr) still writes its file
// when reported -- the JSON output does not depend on the stderr tables.
TEST_F(TimingTest, JsonOnlyStillWritesFile)
{
  std::string path =
    std::string(testing::TempDir()) + "pony_timing_jsononly.json";

  pony_timers_t* t = pony_timers_create();
  pony_timers_set_json(t, path.c_str());
  pony_timer_start(t, "phases", "reach");
  pony_timer_stop(t, "phases", "reach");
  pony_timers_report(t);
  pony_timers_free(t);

  std::string json = slurp(path.c_str());
  remove(path.c_str());

  ASSERT_FALSE(json.empty());
  EXPECT_NE(std::string::npos, json.find("\"reach\""));
}


// pony_timers_set_report_meta must surface the build status, compiler version
// and target triple as top-level JSON fields, so a stored file describes the
// build it came from. Without status a failed build's JSON is indistinguishable
// from a fast successful one; dropping any field emission drops its substring.
TEST_F(TimingTest, MetaFieldsAppearInJson)
{
  std::string path =
    std::string(testing::TempDir()) + "pony_timing_meta.json";

  pony_timers_t* t = pony_timers_create();
  pony_timers_set_json(t, path.c_str());
  pony_timers_set_report_meta(t, false, "9.9.9-abc",
    "x86_64-unknown-linux-gnu");
  pony_timer_start(t, "phases", "parse");
  pony_timer_stop(t, "phases", "parse");
  pony_timers_report(t);
  pony_timers_free(t);

  std::string json = slurp(path.c_str());
  remove(path.c_str());

  ASSERT_FALSE(json.empty());
  EXPECT_NE(std::string::npos, json.find("\"status\": \"failed\""));
  EXPECT_NE(std::string::npos, json.find("\"version\": \"9.9.9-abc\""));
  EXPECT_NE(std::string::npos,
    json.find("\"triple\": \"x86_64-unknown-linux-gnu\""));
  EXPECT_NE(std::string::npos, json.find("\"elapsed_wall\""));
}


// With no metadata set, status defaults to "ok" and the optional version and
// triple fields are omitted rather than emitted empty (a NULL triple writes no
// field at all).
TEST_F(TimingTest, DefaultMetaIsOkAndOmitsVersionTriple)
{
  std::string path =
    std::string(testing::TempDir()) + "pony_timing_defmeta.json";

  pony_timers_t* t = pony_timers_create();
  pony_timers_set_json(t, path.c_str());
  pony_timer_start(t, "phases", "parse");
  pony_timer_stop(t, "phases", "parse");
  pony_timers_report(t);
  pony_timers_free(t);

  std::string json = slurp(path.c_str());
  remove(path.c_str());

  ASSERT_FALSE(json.empty());
  EXPECT_NE(std::string::npos, json.find("\"status\": \"ok\""));
  EXPECT_EQ(std::string::npos, json.find("\"version\""));
  EXPECT_EQ(std::string::npos, json.find("\"triple\""));
}


// JSON numbers must use '.' regardless of the process LC_NUMERIC: %f honours a
// comma-decimal locale and would emit invalid JSON, the integer formatting in
// json_print_seconds does not. Catching the %f regression requires actually
// being in a comma-decimal locale, so this skips (rather than false-passing)
// when the platform has none installed -- in a "C"-only environment even %f
// prints '.', so a passing assertion there would prove nothing.
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

  std::string path =
    std::string(testing::TempDir()) + "pony_timing_locale.json";

  pony_timers_t* t = pony_timers_create();
  pony_timers_set_json(t, path.c_str());
  pony_timer_start(t, "phases", "parse");
  pony_timer_stop(t, "phases", "parse");
  pony_timers_report(t);
  pony_timers_free(t);

  setlocale(LC_NUMERIC, "C"); // restore before other tests run

  std::string json = slurp(path.c_str());
  remove(path.c_str());

  ASSERT_FALSE(json.empty());
  // No number may carry a comma decimal separator: a digit, then ',', then a
  // digit. Structural JSON commas are always followed by a space or newline.
  bool comma_decimal = false;
  for(size_t i = 1; (i + 1) < json.size(); i++)
  {
    if((json[i] == ',') && isdigit((unsigned char)json[i - 1]) &&
      isdigit((unsigned char)json[i + 1]))
    {
      comma_decimal = true;
      break;
    }
  }
  EXPECT_FALSE(comma_decimal);
}
