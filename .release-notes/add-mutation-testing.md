## Add mutation testing

The compiler can now instrument a program for mutation testing with `--mutate`. Each instrumented site can be activated at runtime by setting the `PONY_MUTANT_ID` environment variable, replacing one operator with a variant (e.g., `+` with `-`, `==` with `!=`, or wrapping arithmetic with unsafe arithmetic). A `.mutants.json` file lists every instrumented site with its source location, category, and replacement.

Nine mutation categories are available: `aor` (arithmetic operator replacement), `bor` (boolean operator replacement), `ror` (relational operator replacement), `sor` (shift operator replacement), `uoi` (unary operator insertion), `cr` (constant replacement), `rvr` (return value replacement), `vcr` (void call removal), and `amr` (arithmetic mode replacement). RVR can replace a String return value with a fresh empty String, including mutable and isolated results. CR replaces numeric and character literals written in the source code, not values the compiler computes from them. The other categories skip operators whose operands are all compile-time constants. Use `--mutate-operators=aor,ror` to instrument only specific categories, and `--mutate-scope=file.pony` to restrict instrumentation to specific source files.

The `pony-mutate` tool automates running all mutants against a test binary and reports which ones survived:

```sh
ponyc --mutate -b myapp .
pony-mutate ./myapp
```

Use `--format=json` to print a Stryker JSON report, or `--json-file=report.json` to write one alongside the terminal report. JSON reports include source contents, so the original source files must remain readable as UTF-8 text at their recorded paths. Reports are also written when no mutants are selected or covered.

In generic types and methods, the same mutation uses one ID across instantiations of the same implementing type definition. Default methods inherited by different implementing types have separate mutant IDs for each implementing type. The `shared` field in `.mutants.json` marks sites in generic code, even when only one instantiation is compiled. Coverage is recorded separately for each replacement and only when an instantiation that supports it runs. For example, running a floating-point instantiation does not mark an integer-only complement replacement as covered, so `pony-mutate` reports no coverage for that replacement unless an integer instantiation runs.
