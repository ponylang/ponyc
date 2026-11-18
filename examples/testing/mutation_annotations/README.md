# Mutation category annotations

Ledger uses type-level `nomutate` to disable instrumentation by default. Its `affordable` method opts into arithmetic operator replacement (`aor`) and relational operator replacement (`ror`).

Run these commands from this directory to compile the example and inspect its mutant categories.

```sh
ponyc --mutate --mutate-scope=main.pony .
jq -r '.mutants[].category' mutation_annotations.mutants.json | sort -u
```

The manifest lists `aor` and `ror`. Adding `--mutate-operators=ror,rvr` to the compile command leaves only `ror`.

Use `mutate_only(...)` to select categories and `mutate_except(...)` to exclude categories. Both accept a comma-separated list on concrete types and methods with bodies. See `--mutate-operators` in `ponyc --help` for the category names. A selector must be the declaration's only mutation annotation.

A type-level `mutate_only(...)` or `mutate_except(...)` sets a hard limit for its methods. The compiler rejects a method's `mutate_only(...)` if it requests a category outside that limit, including in builds without `--mutate`. A method-level `mutate_except(...)` removes categories from the type's selection.

A method-level `nomutate` suppresses its body, including nested lambdas. With type-level `nomutate`, a method can opt in with `mutate_only(...)`. A method declared inside an anonymous object inherits the enclosing method's category restrictions.

When a trait or interface supplies a default body, the annotation on the supplying method takes precedence over `nomutate` on a receiving bodyless signature. The signature's annotation still applies to its parameter defaults. The command-line filters can narrow the selection further.
