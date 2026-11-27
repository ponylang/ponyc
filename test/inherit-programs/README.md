# Inherited-method programs

Each directory here is a Pony program about methods a type inherits from a
trait or interface: default method bodies, the object literals and lambdas
written in them, their default arguments, and the errors ponyc reports for
them. Each program either compiles and prints what its `expected-stdout.txt`
holds, or fails to compile with the errors in its `expected-compile-errors.txt`.
Each program's main file (usually `main.pony`) starts with a comment saying
what the program checks and what it expects.

The suite runs with the full-program runner, the same way as
`test/full-program-tests`, and every program is compiled with `--checktree`:

```bash
ctest --preset debug -R inherit-programs
```

## Expected files

Each program has an `expected-stdout.txt` or an `expected-compile-errors.txt`,
as described in `test/full-program-tests/README.md`.

## Groups

The runner treats each directory as one test and doesn't look below it, so
groups are a prefix on the directory name. A new program goes in the group of
the behavior its comment describes. Some programs declare no trait or
interface: they check the same compiler code with plain types, methods and
literals.

- `name-binding-`: names in an inherited method body mean what they mean where
  the body is written: types, values, packages, `use` aliases, methods of other
  types (#6710, #6711, #6725, #6729).
- `signature-`: combining inherited signatures and bodies: comparing two
  declarations of one method, guards and specializations, bodies from another
  trait, ambiguous bodies, which provider supplies a body (#6722, #6728, #6733).
- `privacy-`: private types and methods in inherited code, judged in the package
  where the code is written, including when they arrive through type arguments
  and aliases.
- `narrowing-`: `iftype` and guard narrowing in inherited code, branches that
  can't run for the inheriting type, members found only through a narrowing,
  and specializations that can't run (#6735).
- `diagnostics-`: what errors in inherited code report: the note naming the
  inheriting type, redundancy errors not repeated for each inheriting type, and
  how many errors are reported (related to #6724, which asks for each error
  once; errors in inherited code are still reported for each inheriting type).
- `object-literal-`: object literals and lambdas written in ordinary code: the
  default methods they inherit (#6731), what they capture, and their fields
  and result types.
- `trait-literal-`: object literals and lambdas written in a trait's methods:
  the classes they make for the trait and for each inheriting type, literals
  whose types grow, literals that refer to their own trait, and declaration
  order.
- `literal-identity-`: when two literals are the same type: `is`, `digestof`,
  shared classes, and the type parameters a literal takes (#6736).
- `default-argument-`: default arguments of inherited methods: where they are
  checked, which method's default a call uses, and defaults that depend on
  themselves.
- `source-location-`: `__loc` in inherited code and in anonymous classes.
- `mutation-`: inherited methods whose signatures carry mutation-testing
  annotations such as `\nomutate\`. The programs compile without `--mutate`,
  so they check that the annotations don't change which body or class a type
  gets; the `mutation-*` ctests compile with `--mutate`.
- `debug-info-`: code written in another file than the type that runs it:
  inherited method bodies and default arguments; the programs compile and
  print.

## Lint

`.pony-lint.json` turns off the style and safety rules here, as in
`test/full-program-tests`: the programs are written to exercise the compiler,
not to follow the style guide.
