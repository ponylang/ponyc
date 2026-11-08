## Add mutation testing

The compiler can now instrument a program for mutation testing with `--mutate`. Each instrumented site can be activated at runtime by setting the `PONY_MUTANT_ID` environment variable, replacing one operator with a variant (e.g., `+` with `-`, `==` with `!=`, or wrapping arithmetic with unsafe arithmetic). A `.mutants.json` file lists every instrumented site with its source location, category, and replacement.

Nine mutation categories are available: `aor` (arithmetic operator replacement), `bor` (boolean operator replacement), `ror` (relational operator replacement), `sor` (shift operator replacement), `uoi` (unary operator insertion), `cr` (constant replacement), `rvr` (return value replacement), `vcr` (void call removal), and `amr` (arithmetic mode replacement). CR replaces numeric and character literals written in the source code, not values the compiler computes from them. The other categories skip operators whose operands are all compile-time constants. Use `--mutate-operators=aor,ror` to instrument only specific categories, and `--mutate-scope=file.pony` to restrict instrumentation to specific source files.

The `pony-mutate` tool automates running all mutants against a test binary and reports which ones survived:

```sh
ponyc --mutate -b myapp .
pony-mutate ./myapp
```
