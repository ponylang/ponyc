#ifndef PASS_TIMING_H
#define PASS_TIMING_H

#include <platform.h>

PONY_EXTERN_C_BEGIN

/** Opaque compile-phase timing context.
 *
 * Backs the --pass-timings / --pass-timings-json options. A NULL context means
 * timing is off; every function below is a no-op on NULL, so instrumentation
 * call sites need no guard of their own. Not thread-safe: all calls for one
 * context must come from a single thread (compilation is single-threaded).
 */
typedef struct pony_timers_t pony_timers_t;

// Timer group ids -- the single source of truth for the three report groups,
// shared by the instrumentation call sites and group_description().
#define PONY_TIMER_GROUP_PHASES "phases"
#define PONY_TIMER_GROUP_PACKAGES "packages"
#define PONY_TIMER_GROUP_PACKAGE_PASSES "package_passes"

/** Create a timing context that collects timings. Choose its output with
 * pony_timers_enable_stderr (the tables) and/or pony_timers_set_json (a file).
 */
pony_timers_t* pony_timers_create();

/** Free a timing context (NULL-safe).
 *
 * Must be called before LLVM's global state is torn down (llvm_shutdown):
 * destroying a TimerGroup unlinks it from an LLVM-global list.
 */
void pony_timers_free(pony_timers_t* t);

/** Print the human-readable tables to stderr when reporting (NULL-safe). Off by
 * default: a context set up only for JSON output stays quiet on stderr.
 */
void pony_timers_enable_stderr(pony_timers_t* t);

/** Write the report as JSON to `path` when reporting (NULL-safe). Copies
 * `path`. Independent of the stderr tables.
 */
void pony_timers_set_json(pony_timers_t* t, const char* path);

/** Attach report metadata, emitted as top-level JSON fields (NULL-safe).
 * `build_ok` records whether compilation succeeded -- without it a failed
 * build's JSON is indistinguishable from a fast successful one. `version` and
 * `triple` are copied; `triple` may be NULL when the target was never resolved
 * (the caller has no triple yet), in which case no triple field is emitted.
 */
void pony_timers_set_report_meta(pony_timers_t* t, bool build_ok,
  const char* version, const char* triple);

/** Start a named region within a named group (NULL-safe).
 *
 * Regions accumulate by (group, name) across calls, so many start/stop pairs
 * for one name fold into a single row. Re-entering the same (group, name) --
 * e.g. the expr pass re-entering itself on a lazily-instantiated generic -- is
 * handled: the region's inclusive span is counted once, not once per nesting
 * level. Distinct regions that overlap in time are each counted in full, so
 * their totals can sum to more than the elapsed wall-clock time. `group` and
 * `name` are copied, so they need only be valid for the duration of the call.
 */
void pony_timer_start(pony_timers_t* t, const char* group, const char* name);

/** Stop a named region within a named group (NULL-safe). Must pair with a
 * prior pony_timer_start for the same (group, name).
 */
void pony_timer_stop(pony_timers_t* t, const char* group, const char* name);

/** As pony_timer_start, but the region name is composed from two parts as
 * "name_a (name_b)" -- e.g. package and pass. name_a and name_b are copied, so
 * they need only be valid for the duration of the call.
 */
void pony_timer_start_pair(pony_timers_t* t, const char* group,
  const char* name_a, const char* name_b);

/** Stop a region started with pony_timer_start_pair. The two parts must match
 * the start call so the composed name keys the same region.
 */
void pony_timer_stop_pair(pony_timers_t* t, const char* group,
  const char* name_a, const char* name_b);

/** Emit the report (NULL-safe): the stderr tables if pony_timers_enable_stderr
 * was called, and the JSON file if pony_timers_set_json was called.
 */
void pony_timers_report(pony_timers_t* t);

/** Test-only introspection: current re-entrancy depth of a region -- 0 when the
 * region does not exist or the context is NULL. Lets tests assert the depth
 * guard's bookkeeping directly, without depending on wall-clock magnitudes or on
 * whether the linked LLVM was built with assertions.
 */
unsigned int pony_timer_depth(pony_timers_t* t, const char* group,
  const char* name);

PONY_EXTERN_C_END

#endif
