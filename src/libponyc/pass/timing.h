#ifndef PASS_TIMING_H
#define PASS_TIMING_H

#include <platform.h>

PONY_EXTERN_C_BEGIN

/** Opaque front-end timing context.
 *
 * Backs the --pass-timings / --pass-timings-json options. A NULL context means
 * timing is off; every function below is a no-op on NULL, so instrumentation
 * call sites need no guard of their own. A site on a hot path may still test
 * the context first, to skip the work of building the arguments. Not
 * thread-safe: all calls for one context must come from a single thread
 * (compilation is single-threaded).
 */
typedef struct pass_timers_t pass_timers_t;

/** Create a timing context. Choose its output with pass_timers_enable_table (a
 * stderr table) and/or pass_timers_set_json (a file); a context with neither
 * collects timings and prints nothing.
 */
pass_timers_t* pass_timers_create();

/** Free a timing context (NULL-safe).
 *
 * Must be called before LLVM's global state is torn down (llvm_shutdown).
 * Destroying a Timer unlinks it from its TimerGroup, which takes an LLVM-global
 * lock without checking whether the globals still exist; after llvm_shutdown
 * that reconstructs them and re-registers their command-line options.
 */
void pass_timers_free(pass_timers_t* t);

/** Print the table to stderr when reporting (NULL-safe). Off by default, so a
 * context set up only for JSON output stays quiet on stderr.
 */
void pass_timers_enable_table(pass_timers_t* t);

/** Write the report as JSON to `path` when reporting (NULL-safe). Copies
 * `path`. Independent of the stderr table. Returns false if `path` cannot be
 * written, having reported why.
 *
 * The file is built at `<path>.tmp` and renamed over `path` once it is
 * complete, so an unwritable path is diagnosed here rather than after the
 * build, and a file from an earlier run survives a failed one intact instead of
 * being truncated or half-rewritten.
 */
bool pass_timers_set_json(pass_timers_t* t, const char* path);

/** Attach report metadata, emitted as top-level JSON fields (NULL-safe).
 * `build_ok` records whether compilation succeeded -- without it a failed
 * build's JSON is indistinguishable from a fast successful one. `triple` is
 * copied, and may be NULL, in which case no triple field is emitted.
 */
void pass_timers_set_report_meta(pass_timers_t* t, bool build_ok,
  const char* triple);

/** Start timing a pass on a package (NULL-safe).
 *
 * Regions accumulate by (package, pass) across calls, so many start/stop pairs
 * for one package and pass fold into a single row.
 *
 * Spans are inclusive and nest: loading a dependency runs its parse and syntax
 * inside the importing package's scope span, so that time is counted in both
 * rows. Distinct regions that overlap are each counted in full, so the rows can
 * sum to more than the elapsed wall-clock time.
 *
 * A region must not be started while it is already running, which llvm::Timer
 * forbids. No compilation does so today, because the only nested package visit
 * loads a package that is not yet in the program. The depth counter enforces
 * the rule regardless: the vendored LLVM is a Release build, so its own assert
 * would not catch a violation.
 *
 * `package` and `pass` are copied, so they need only be valid for the duration
 * of the call.
 */
void pass_timers_start(pass_timers_t* t, const char* package, const char* pass);

/** Stop a region started with pass_timers_start (NULL-safe). The two names must
 * match the start call so they key the same region. A stop with no matching
 * start is ignored, and creates no row.
 */
void pass_timers_stop(pass_timers_t* t, const char* package, const char* pass);

/** Emit the report (NULL-safe): the stderr table if pass_timers_enable_table
 * was called, and the JSON file if pass_timers_set_json was. Returns false if
 * the JSON file could not be written, having reported why.
 *
 * Call once. A second call writes the JSON a second time.
 */
bool pass_timers_report(pass_timers_t* t);

/** Test-only introspection: current nesting depth of a region -- 0 when the
 * region does not exist or the context is NULL.
 */
unsigned int pass_timers_depth(pass_timers_t* t, const char* package,
  const char* pass);

/** Test-only introspection: wall seconds recorded for a region -- 0 when the
 * region does not exist or the context is NULL.
 */
double pass_timers_wall(pass_timers_t* t, const char* package,
  const char* pass);

/** Test-only: format `v` seconds into `buf` exactly as the JSON writer does.
 *
 * Both of the branches a test needs this for are reachable. The rounding carry
 * fires whenever the fractional part rounds up to a whole second. The clamp
 * fires on a negative duration, which LLVM's wall clock can report because it
 * reads a realtime clock rather than a monotonic one, so a backwards step of
 * the system clock during a build makes the elapsed time negative.
 */
void pass_timers_format_seconds(double v, char* buf, size_t size);

PONY_EXTERN_C_END

#endif
