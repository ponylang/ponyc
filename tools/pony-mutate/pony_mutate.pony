"""
# pony-mutate

A mutation testing runner for Pony programs compiled with `--mutate`.

Reads the `.mutants.json` info file written by ponyc, runs the test binary
for each mutant, and reports which mutants survived (meaning no test caught
the change). Surviving mutants indicate gaps in test coverage.

## Quick start

Compile with mutation testing enabled:

```sh
ponyc --mutate -b mytest myproject
```

Run the mutation suite:

```sh
pony-mutate ./mytest
```

The runner reads `mytest.mutants.json`, executes a baseline run, then
runs the binary once per mutant with `PONY_MUTANT_ID` set. Each mutant
is classified as killed (test failed), survived (test passed — a gap),
or timed out. The terminal report lists survived mutants with source
locations.

Use `--format=json` for a Stryker JSON report on stdout, or
`--json-file=report.json` to write one alongside either stdout format.
JSON reports include source contents, so the original source files must
remain readable as UTF-8 text at the paths recorded in the info file.
"""
