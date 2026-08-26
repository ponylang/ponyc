## Add --pass-timings/--pass-timings-json for profiling compiler pass times

`ponyc` can now report how long each front-end pass takes on each package, to help you find the slow part of a slow build.

`--pass-timings` prints one table to stderr after a build, with a row per package and pass and its wall, user, and system time. A row like `mylib/thing (expr)` is the time type checking spent on that package, so you can see which pass on which package is slow rather than only that the build is slow overall.

Pass `--pass-timings-json=FILE` to write the same timings as JSON for scripting or tracking over time; on its own it writes only the file, so combine it with `--pass-timings` if you also want the table on stderr. Each JSON file also records whether the build succeeded, the compiler version and target triple, and the total elapsed time, so a stored file describes the build it came from.

Only the front-end passes are timed. Compiling C shims, plugin passes, reach, codegen, LLVM optimisation and linking are not, so the rows can account for a small share of a long build. The table prints the elapsed wall-clock time alongside the rows so you can see what share they cover.

Times are inclusive, so rows can overlap: loading a package runs its parse and syntax inside the importing package's scope row, and that time is counted in both. Time spent instantiating a generic is counted in the row for the pass that triggered it, so a package's `expr` row includes the earlier passes re-run on its instantiations.

```
ponyc --pass-timings my_package
ponyc --pass-timings --pass-timings-json=timings.json my_package
```

Compiling several packages in one `ponyc` invocation writes a single report covering all of them, with rows summed across them.

The JSON file is written when the build ends, so a build you interrupt produces no timings and leaves any file from an earlier run untouched.
