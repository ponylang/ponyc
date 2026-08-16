## Add --pass-timings/--pass-timings-json for profiling compiler pass times

`ponyc` can now report how long each phase of compilation takes, to help you find where a slow build is spending its time.

Pass `--pass-timings` to print three tables to stderr after a build: the wall/user/system time for every compiler phase (parse, sugar, expr, reach, optimize, link, and so on); the total front-end time per package, so you can see which package is most expensive to compile; and a per-package, per-pass breakdown, so you can see exactly which pass on which package is slow (for example, type checking `expr` on a particular package). Pass `--pass-timings-json=FILE` to write the same numbers as JSON for scripting or tracking over time; on its own it writes only the file, so combine it with `--pass-timings` if you also want the tables. Each JSON file also records whether the build succeeded, the compiler version and target triple, and the total elapsed time, so a stored file describes the build it came from.

Phase times are inclusive, so they can overlap: type checking re-runs earlier passes on each generic instantiation, and that time is counted under both. Per-phase figures can therefore sum to more than the elapsed wall-clock time; the per-package total reflects the actual elapsed front-end time.

```
ponyc --pass-timings --pass-timings-json=timings.json my_package
```
