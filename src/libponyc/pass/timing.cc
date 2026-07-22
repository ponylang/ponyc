#include "timing.h"

#include <llvm/Support/Timer.h>
#include <llvm/Support/raw_ostream.h>

#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Invariant:
//   Timing results are ordered by descending wall time. Currently the sorting
//   is handled by llvm::TimerGroup.
namespace
{
  struct phase_timer_t
  {
    std::unique_ptr<llvm::Timer> timer;
    // Re-entrancy depth. A phase can re-enter itself (the expr pass runs the
    // whole pass sequence, incl. expr, on a lazily-instantiated generic), and
    // an already-running llvm::Timer must not be started again. Only the
    // outermost start/stop pair drives the timer, so inclusive wall time is
    // counted once.
    unsigned depth = 0;
    // Set when the region was named via the pair API. Keeps the two parts so
    // JSON consumers get them as separate fields instead of splitting the
    // composed "name_a (name_b)" display string.
    bool is_pair = false;
    std::string name_a;
    std::string name_b;
  };

  struct timer_group_t
  {
    std::unique_ptr<llvm::TimerGroup> group;
    std::map<std::string, phase_timer_t> timers;
    std::vector<std::string> order; // name insertion order, for stable JSON
  };
}

struct pony_timers_t
{
  std::map<std::string, timer_group_t> groups;
  std::vector<std::string> group_order; // group insertion order, for the report
  std::string json_path;
  bool has_json = false;
  // Print the human-readable tables to stderr. Set by --pass-timings; a
  // --pass-timings-json alone collects and writes JSON without the stderr dump.
  bool print_report = false;
  // Report metadata, emitted as top-level JSON fields (set via
  // pony_timers_set_report_meta). start_wall is captured at create so the JSON
  // can report the elapsed wall time; build_ok distinguishes a failed build's
  // JSON from a fast successful one; version/triple describe what produced it.
  double start_wall = 0.0;
  bool build_ok = true;
  std::string version;
  bool has_triple = false;
  std::string triple;
};

static const char* group_description(const std::string& id)
{
  if(id == PONY_TIMER_GROUP_PHASES)
    return "Pony compiler phases";
  if(id == PONY_TIMER_GROUP_PACKAGES)
    return "Pony front-end time by package";
  if(id == PONY_TIMER_GROUP_PACKAGE_PASSES)
    return "Pony front-end time by package and pass";
  return id.c_str();
}

static timer_group_t& get_group(pony_timers_t* t, const char* group)
{
  std::string gid(group);
  auto it = t->groups.find(gid);
  if(it != t->groups.end())
    return it->second;

  t->group_order.push_back(gid);
  timer_group_t& gs = t->groups[gid];
  // PrintOnExit = false: pony_timers_report prints explicitly. The default
  // (true) would make LLVM print the group a second time at shutdown.
  gs.group = std::make_unique<llvm::TimerGroup>(gid, group_description(gid),
    false);
  return gs;
}

static phase_timer_t& get_timer(timer_group_t& gs, const std::string& nid)
{
  auto it = gs.timers.find(nid);
  if(it != gs.timers.end())
    return it->second;

  gs.order.push_back(nid);
  phase_timer_t& pt = gs.timers[nid];
  pt.timer = std::make_unique<llvm::Timer>(nid, nid, *gs.group);
  return pt;
}

static void bump_start(phase_timer_t& pt)
{
  if(pt.depth++ == 0)
    pt.timer->startTimer();
}

static void bump_stop(phase_timer_t& pt)
{
  if(pt.depth == 0) // unbalanced stop; ignore rather than corrupt the timer
    return;

  if(--pt.depth == 0)
    pt.timer->stopTimer();
}

pony_timers_t* pony_timers_create()
{
  pony_timers_t* t = new pony_timers_t();
  t->start_wall = llvm::TimeRecord::getCurrentTime(false).getWallTime();
  return t;
}

void pony_timers_free(pony_timers_t* t)
{
  delete t;
}

void pony_timers_set_json(pony_timers_t* t, const char* path)
{
  if(t == nullptr)
    return;

  t->json_path = path;
  t->has_json = true;
}

void pony_timers_enable_stderr(pony_timers_t* t)
{
  if(t == nullptr)
    return;

  t->print_report = true;
}

void pony_timers_set_report_meta(pony_timers_t* t, bool build_ok,
  const char* version, const char* triple)
{
  if(t == nullptr)
    return;

  t->build_ok = build_ok;
  t->version = (version != nullptr) ? version : "";
  t->has_triple = (triple != nullptr);
  t->triple = t->has_triple ? triple : "";
}

void pony_timer_start(pony_timers_t* t, const char* group, const char* name)
{
  if(t == nullptr)
    return;

  bump_start(get_timer(get_group(t, group), std::string(name)));
}

void pony_timer_stop(pony_timers_t* t, const char* group, const char* name)
{
  if(t == nullptr)
    return;

  bump_stop(get_timer(get_group(t, group), std::string(name)));
}

unsigned int pony_timer_depth(pony_timers_t* t, const char* group,
  const char* name)
{
  if(t == nullptr)
    return 0;

  // find(), not get_group/get_timer: introspection must not create a region as
  // a side effect, so an absent region reads as depth 0.
  auto git = t->groups.find(std::string(group));
  if(git == t->groups.end())
    return 0;

  auto tit = git->second.timers.find(std::string(name));
  if(tit == git->second.timers.end())
    return 0;

  return tit->second.depth;
}

static std::string pair_name(const char* name_a, const char* name_b)
{
  return std::string(name_a) + " (" + name_b + ")";
}

void pony_timer_start_pair(pony_timers_t* t, const char* group,
  const char* name_a, const char* name_b)
{
  if(t == nullptr)
    return;

  phase_timer_t& pt = get_timer(get_group(t, group), pair_name(name_a, name_b));
  pt.is_pair = true;
  pt.name_a = name_a;
  pt.name_b = name_b;
  bump_start(pt);
}

void pony_timer_stop_pair(pony_timers_t* t, const char* group,
  const char* name_a, const char* name_b)
{
  if(t == nullptr)
    return;

  bump_stop(get_timer(get_group(t, group), pair_name(name_a, name_b)));
}

static void json_print_escaped(FILE* f, const char* s)
{
  fputc('"', f);

  for(; *s != '\0'; s++)
  {
    unsigned char c = (unsigned char)*s;

    switch(c)
    {
      case '"':  fputs("\\\"", f); break;
      case '\\': fputs("\\\\", f); break;
      case '\b': fputs("\\b", f); break;
      case '\f': fputs("\\f", f); break;
      case '\n': fputs("\\n", f); break;
      case '\r': fputs("\\r", f); break;
      case '\t': fputs("\\t", f); break;
      default:
        if(c < 0x20)
          fprintf(f, "\\u%04x", c);
        else
          fputc(c, f);
    }
  }

  fputc('"', f);
}

// Format seconds with microsecond precision, locale-independently: integer
// formatting always uses '.', so the JSON stays valid whatever the process
// LC_NUMERIC is (%f would honour it and could emit a ',').
static void json_print_seconds(FILE* f, double v)
{
  if(v < 0.0)
    v = 0.0;

  long long whole = (long long)v;
  long long micros = (long long)llround((v - (double)whole) * 1e6);

  if(micros >= 1000000) // rounding carried into the next second
  {
    whole += 1;
    micros -= 1000000;
  }

  fprintf(f, "%lld.%06lld", whole, micros);
}

static void write_json(pony_timers_t* t)
{
  FILE* f = fopen(t->json_path.c_str(), "w");

  if(f == NULL)
  {
    fprintf(stderr, "ponyc: could not open pass-timings JSON file '%s'\n",
      t->json_path.c_str());
    return;
  }

  fprintf(f, "{\n");

  // Emit each top-level member with a leading separator after the first, so the
  // optional metadata fields and the timer groups compose without a dangling or
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

  if(!t->version.empty())
  {
    member("version");
    json_print_escaped(f, t->version.c_str());
  }

  if(t->has_triple)
  {
    member("triple");
    json_print_escaped(f, t->triple.c_str());
  }

  member("elapsed_wall");
  json_print_seconds(f,
    llvm::TimeRecord::getCurrentTime(false).getWallTime() - t->start_wall);

  for(const std::string& gid : t->group_order)
  {
    const timer_group_t& gs = t->groups.at(gid);

    member(gid.c_str());
    fprintf(f, "[\n");

    bool first_timer = true;

    for(const std::string& nid : gs.order)
    {
      const phase_timer_t& pt = gs.timers.at(nid);
      llvm::TimeRecord tr = pt.timer->getTotalTime();

      if(!first_timer)
        fprintf(f, ",\n");
      first_timer = false;

      fprintf(f, "    {\"name\": ");
      json_print_escaped(f, nid.c_str());
      if(pt.is_pair)
      {
        fprintf(f, ", \"name_a\": ");
        json_print_escaped(f, pt.name_a.c_str());
        fprintf(f, ", \"name_b\": ");
        json_print_escaped(f, pt.name_b.c_str());
      }
      fprintf(f, ", \"wall\": ");
      json_print_seconds(f, tr.getWallTime());
      fprintf(f, ", \"user\": ");
      json_print_seconds(f, tr.getUserTime());
      fprintf(f, ", \"system\": ");
      json_print_seconds(f, tr.getSystemTime());
      fprintf(f, "}");
    }

    fprintf(f, "\n  ]");
  }

  fprintf(f, "\n}\n");

  // A write can fail after a successful open (full disk, quota, I/O error).
  // Report it and drop the partial file rather than leave invalid JSON that a
  // consumer would read as real data.
  bool write_failed = (ferror(f) != 0);

  if(fclose(f) != 0)
    write_failed = true;

  if(write_failed)
  {
    fprintf(stderr, "ponyc: failed to write pass-timings JSON file '%s'\n",
      t->json_path.c_str());
    remove(t->json_path.c_str());
  }
}

void pony_timers_report(pony_timers_t* t)
{
  if(t == nullptr)
    return;

  // --pass-timings prints the tables; --pass-timings-json alone writes only the
  // file, so a scripting caller gets no stderr noise.
  if(t->print_report)
  {
    for(const std::string& gid : t->group_order)
      t->groups.at(gid).group->print(llvm::errs());

    // The phase percentages/total assume a partition, but phase times are
    // inclusive and overlap under re-entrancy, so they can exceed the elapsed
    // time. Flag it where the numbers are, not only in the docs.
    llvm::errs() << "Note: phase times are inclusive and can overlap "
      "(re-entrant passes), so per-phase percentages and total may exceed the "
      "elapsed wall-clock time; the per-package total reflects elapsed time.\n";
    llvm::errs().flush();
  }

  if(t->has_json)
    write_json(t);
}
