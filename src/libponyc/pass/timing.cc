#include "timing.h"
#include "pony_version.h"

#include "llvm_config_begin.h"

#include <llvm/Support/Timer.h>
#include <llvm/Support/raw_ostream.h>

#include "llvm_config_end.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <string>

namespace
{
  struct pass_timer_t
  {
    std::unique_ptr<llvm::Timer> timer;
    // Nesting depth, so only the outermost start/stop pair drives the timer and
    // an inclusive span is counted once. llvm::Timer forbids starting a running
    // timer; see pass_timers_start in timing.h for when that can arise.
    unsigned depth = 0;
    // The two parts the region is keyed by. Kept so JSON consumers get them as
    // separate fields instead of splitting the composed display name.
    std::string package;
    std::string pass;
  };
}

struct pass_timers_t
{
  std::unique_ptr<llvm::TimerGroup> group;
  // Keyed by composed display name. std::map, so the JSON rows come out in a
  // stable order whatever order compilation created them in. That order is by
  // name; the stderr table is sorted by wall time by LLVM instead.
  std::map<std::string, pass_timer_t> timers;
  bool print_table = false;
  std::string json_path;
  std::string json_tmp;
  bool has_json = false;
  // Number of regions currently running, and whether two were ever running at
  // once. The report explains nesting only when nesting actually happened.
  unsigned running = 0;
  bool observed_nesting = false;
  // Report metadata. start_wall is captured at create so the report can give
  // the elapsed wall time; build_ok distinguishes a failed build's JSON from a
  // fast successful one; triple describes what it was built for.
  double start_wall = 0.0;
  bool build_ok = true;
  bool has_triple = false;
  std::string triple;
};

static std::string region_name(const char* package, const char* pass)
{
  return std::string(package) + " (" + pass + ")";
}

static pass_timer_t& get_timer(pass_timers_t* t, const char* package,
  const char* pass)
{
  std::string nid = region_name(package, pass);
  auto it = t->timers.find(nid);
  if(it != t->timers.end())
    return it->second;

  pass_timer_t& pt = t->timers[nid];
  pt.timer = std::make_unique<llvm::Timer>(nid, nid, *t->group);
  pt.package = package;
  pt.pass = pass;
  return pt;
}

// Look a region up without creating it.
static pass_timer_t* find_timer(pass_timers_t* t, const char* package,
  const char* pass)
{
  if(t == nullptr)
    return nullptr;

  auto it = t->timers.find(region_name(package, pass));
  return (it != t->timers.end()) ? &it->second : nullptr;
}

pass_timers_t* pass_timers_create()
{
  pass_timers_t* t = new pass_timers_t();
  // PrintOnExit = false: pass_timers_report prints explicitly. The default
  // (true) would make LLVM print the group a second time at shutdown.
  t->group = std::make_unique<llvm::TimerGroup>("package_passes",
    "Pony front-end time by package and pass", false);
  t->start_wall = llvm::TimeRecord::getCurrentTime(false).getWallTime();
  return t;
}

void pass_timers_free(pass_timers_t* t)
{
  delete t;
}

void pass_timers_enable_table(pass_timers_t* t)
{
  if(t == nullptr)
    return;

  t->print_table = true;
}

bool pass_timers_set_json(pass_timers_t* t, const char* path)
{
  if(t == nullptr)
    return true;

  t->json_path = path;
  t->json_tmp = t->json_path + ".tmp";

  // Prove the path is writable now, so a typo costs nothing instead of a whole
  // build, then close and remove the probe: holding it open until the report
  // would leave it behind if the build were interrupted.
  FILE* probe = fopen(t->json_tmp.c_str(), "w");

  if(probe == nullptr)
  {
    fprintf(stderr, "ponyc: cannot write pass-timings JSON file '%s'\n", path);
    return false;
  }

  fclose(probe);
  remove(t->json_tmp.c_str());

  t->has_json = true;
  return true;
}

void pass_timers_set_report_meta(pass_timers_t* t, bool build_ok,
  const char* triple)
{
  if(t == nullptr)
    return;

  t->build_ok = build_ok;
  t->has_triple = (triple != nullptr);
  t->triple = t->has_triple ? triple : "";
}

void pass_timers_start(pass_timers_t* t, const char* package, const char* pass)
{
  if(t == nullptr)
    return;

  pass_timer_t& pt = get_timer(t, package, pass);

  if(pt.depth++ == 0)
  {
    if(t->running > 0)
      t->observed_nesting = true;

    t->running++;
    pt.timer->startTimer();
  }
}

void pass_timers_stop(pass_timers_t* t, const char* package, const char* pass)
{
  // Look the region up without creating it: a stop with no matching start must
  // leave no trace, or it shows up as a zero row in the JSON while the table
  // (which skips timers that never ran) omits it.
  pass_timer_t* pt = find_timer(t, package, pass);

  if((pt == nullptr) || (pt->depth == 0))
    return;

  if(--pt->depth == 0)
  {
    t->running--;
    pt->timer->stopTimer();
  }
}

unsigned int pass_timers_depth(pass_timers_t* t, const char* package,
  const char* pass)
{
  const pass_timer_t* pt = find_timer(t, package, pass);
  return (pt != nullptr) ? pt->depth : 0;
}

double pass_timers_wall(pass_timers_t* t, const char* package,
  const char* pass)
{
  const pass_timer_t* pt = find_timer(t, package, pass);
  return (pt != nullptr) ? pt->timer->getTotalTime().getWallTime() : 0.0;
}

// Emit `s` as a JSON string. Bytes JSON requires escaped are escaped, and
// ill-formed UTF-8 becomes U+FFFD: a package name comes from the filesystem,
// where a name is any NUL-free byte string, and a raw ill-formed byte would
// produce a file a strict parser rejects.
static void json_print_escaped(FILE* f, const char* s)
{
  static const char REPLACEMENT[] = "\xEF\xBF\xBD";

  fputc('"', f);

  const unsigned char* p = (const unsigned char*)s;

  while(*p != '\0')
  {
    unsigned char c = *p;

    switch(c)
    {
      case '"':  fputs("\\\"", f); p++; continue;
      case '\\': fputs("\\\\", f); p++; continue;
      case '\b': fputs("\\b", f); p++; continue;
      case '\f': fputs("\\f", f); p++; continue;
      case '\n': fputs("\\n", f); p++; continue;
      case '\r': fputs("\\r", f); p++; continue;
      case '\t': fputs("\\t", f); p++; continue;
      default: break;
    }

    if(c < 0x20)
    {
      fprintf(f, "\\u%04x", c);
      p++;
      continue;
    }

    if(c < 0x80)
    {
      fputc((char)c, f);
      p++;
      continue;
    }

    size_t len;
    uint32_t cp;

    if((c & 0xE0) == 0xC0)
    {
      len = 2;
      cp = c & 0x1Fu;
    } else if((c & 0xF0) == 0xE0) {
      len = 3;
      cp = c & 0x0Fu;
    } else if((c & 0xF8) == 0xF0) {
      len = 4;
      cp = c & 0x07u;
    } else {
      // A continuation byte with nothing to continue, or a length no longer
      // permitted.
      fputs(REPLACEMENT, f);
      p++;
      continue;
    }

    // Stops at the terminator too, since NUL is not a continuation byte.
    size_t i;

    for(i = 1; i < len; i++)
    {
      if((p[i] & 0xC0) != 0x80)
        break;

      cp = (cp << 6) | (p[i] & 0x3Fu);
    }

    bool valid = (i == len)
      && !((len == 2) && (cp < 0x80))        // overlong
      && !((len == 3) && (cp < 0x800))       // overlong
      && !((len == 4) && (cp < 0x10000))     // overlong
      && !((cp >= 0xD800) && (cp <= 0xDFFF)) // surrogate half
      && (cp <= 0x10FFFF);

    if(!valid)
    {
      fputs(REPLACEMENT, f);
      p++;
      continue;
    }

    fwrite(p, 1, len, f);
    p += len;
  }

  fputc('"', f);
}

// Format seconds with microsecond precision, locale-independently: integer
// formatting always uses '.', so the JSON stays valid whatever the process
// LC_NUMERIC is (%f would honour it and could emit a ',').
static void format_seconds(double v, char* buf, size_t size)
{
  // Negated so that NaN clamps too -- every comparison against NaN is false, so
  // `v < 0.0` would let one through into the conversion below, where it is
  // undefined. The upper bound keeps the conversion in range for the same
  // reason.
  if(!(v >= 0.0))
    v = 0.0;

  if(v > 1e15)
    v = 1e15;

  long long whole = (long long)v;
  long long micros = (long long)llround((v - (double)whole) * 1e6);

  if(micros >= 1000000) // rounding carried into the next second
  {
    whole += 1;
    micros -= 1000000;
  }

  snprintf(buf, size, "%lld.%06lld", whole, micros);
}

static void json_print_seconds(FILE* f, double v)
{
  char buf[32]; // "%lld.%06lld": <=16-digit whole + '.' + 6 digits + NUL
  format_seconds(v, buf, sizeof(buf));
  fputs(buf, f);
}

void pass_timers_format_seconds(double v, char* buf, size_t size)
{
  format_seconds(v, buf, size);
}

static double elapsed_wall(pass_timers_t* t)
{
  return llvm::TimeRecord::getCurrentTime(false).getWallTime() - t->start_wall;
}

static double rows_total_wall(pass_timers_t* t)
{
  double total = 0.0;

  for(const auto& entry : t->timers)
    total += entry.second.timer->getTotalTime().getWallTime();

  return total;
}

// Replace `to` with `from`, atomically where the platform offers it.
static bool replace_file(const char* from, const char* to)
{
#ifdef PLATFORM_IS_WINDOWS
  return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING) != 0;
#else
  return rename(from, to) == 0;
#endif
}

static bool write_json(pass_timers_t* t)
{
  // Built beside the target and renamed over it at the end, so a reader never
  // sees a half-written file and a failure here leaves the previous one intact.
  FILE* f = fopen(t->json_tmp.c_str(), "w");

  if(f == nullptr)
  {
    fprintf(stderr, "ponyc: cannot write pass-timings JSON file '%s'\n",
      t->json_path.c_str());
    return false;
  }

  fprintf(f, "{\n");

  // Emit each top-level member with a leading separator after the first, so the
  // optional metadata fields and the timings compose without a dangling or
  // missing comma.
  bool first = true;
  auto member = [&](const char* key)
  {
    if(!first)
      fprintf(f, ",\n");
    first = false;
    fprintf(f, "  ");
    json_print_escaped(f, key);
    fprintf(f, ": ");
  };

  member("status");
  json_print_escaped(f, t->build_ok ? "ok" : "failed");

  member("compiler_version");
  json_print_escaped(f, PONY_VERSION);

  if(t->has_triple)
  {
    member("triple");
    json_print_escaped(f, t->triple.c_str());
  }

  member("elapsed_wall");
  json_print_seconds(f, elapsed_wall(t));

  member("package_passes");
  fprintf(f, "[\n");

  bool first_timer = true;

  for(const auto& entry : t->timers)
  {
    const pass_timer_t& pt = entry.second;
    llvm::TimeRecord tr = pt.timer->getTotalTime();

    if(!first_timer)
      fprintf(f, ",\n");
    first_timer = false;

    fprintf(f, "    {\"package\": ");
    json_print_escaped(f, pt.package.c_str());
    fprintf(f, ", \"pass\": ");
    json_print_escaped(f, pt.pass.c_str());
    fprintf(f, ", \"wall\": ");
    json_print_seconds(f, tr.getWallTime());
    fprintf(f, ", \"user\": ");
    json_print_seconds(f, tr.getUserTime());
    fprintf(f, ", \"system\": ");
    json_print_seconds(f, tr.getSystemTime());
    fprintf(f, "}");
  }

  fprintf(f, "\n  ]\n}\n");

  // A write can fail after a successful open (full disk, quota, I/O error).
  bool failed = (ferror(f) != 0);

  if(fclose(f) != 0)
    failed = true;

  if(!failed && !replace_file(t->json_tmp.c_str(), t->json_path.c_str()))
    failed = true;

  if(failed)
  {
    fprintf(stderr, "ponyc: failed to write pass-timings JSON file '%s'\n",
      t->json_path.c_str());
    remove(t->json_tmp.c_str());
    return false;
  }

  return true;
}

static void print_table(pass_timers_t* t)
{
  t->group->print(llvm::errs());

  char rows_buf[32];
  char elapsed_buf[32];
  format_seconds(rows_total_wall(t), rows_buf, sizeof(rows_buf));
  format_seconds(elapsed_wall(t), elapsed_buf, sizeof(elapsed_buf));

  // llvm::TimerGroup's total and percentages treat the rows as a partition of
  // the run. They are not one, in two directions at once, so give the reader
  // the elapsed time to measure the rows against and say which way each effect
  // runs.
  llvm::errs() << "Rows above total " << rows_buf << " s of " << elapsed_buf
    << " s elapsed.\n"
    << "Only the front-end passes are timed. Compiling C shims, plugin passes, "
       "reach, codegen, LLVM optimisation and linking are not, so the rows can "
       "cover far less of a build than the elapsed time.\n";

  if(t->observed_nesting)
    llvm::errs() << "Rows are inclusive and nest -- loading a package runs its "
      "parse and syntax inside the importing package's scope row, counting "
      "that time in both -- so they can also sum to more than the time they "
      "cover.\n";

  llvm::errs().flush();
}

bool pass_timers_report(pass_timers_t* t)
{
  if(t == nullptr)
    return true;

  // A build that collected nothing prints no table rather than an empty one,
  // but still says so, since silence is indistinguishable from an unrecognised
  // option.
  if(t->print_table)
  {
    if(t->timers.empty())
      llvm::errs() << "No pass timings collected.\n";
    else
      print_table(t);
  }

  return t->has_json ? write_json(t) : true;
}
