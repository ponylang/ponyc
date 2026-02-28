## Fix pony-lsp inability to find the standard library

Previously, pony-lsp was unable to locate the Pony standard library on its own. It relied entirely on the `PONYPATH` environment variable to find packages like `builtin`. This meant that while the VS Code extension could work around the issue by configuring the path explicitly, other editors using pony-lsp would fail with errors like "couldn't locate this path in file builtin".

pony-lsp now automatically discovers the standard library by finding its own executable directory and searching for packages relative to it — the same approach that ponyc uses. Since pony-lsp is installed alongside ponyc, the standard library is found without any manual configuration, making pony-lsp work out of the box with any editor.

## Fix persistent HashMap returning incorrect results for None values

The persistent `HashMap` used `None` as an internal sentinel to signal "key not found" in its lookup methods. This collided with user value types that include `None` (e.g., `Map[String, (String | None)]`). Using `HashMap` with a `None` value could lead to errors in user code as "it was none" and "it wasn't present" were impossible to distinguish.

The internal lookup methods now use `error` instead of `None` to signal a missing key, so all value types work correctly.

This is a breaking change for any code that was depending on the previous (incorrect) behavior. For example, code that expected `apply` to raise for keys mapped to `None`, or that relied on `contains` returning `false` for `None`-valued entries, will now see correct results instead.

## Add pony-lint to the ponyc distribution

pony-lint is a text-based linter for Pony source files that checks for style guide violations. It was previously a standalone project and is now distributed with ponyc. This means the linter will track changes in ponyc and will always be up-to-date with the compiler's version of the standard library. pony-lint currently checks for line length, trailing whitespace, hard tabs, and comment spacing violations.

## Fix stack overflow in reachability pass on deeply nested ASTs

The reachability pass traversed AST trees using unbounded recursion, adding a stack frame for each child node. On Windows x86-64, which has a default stack size of 1MB, this could overflow when compiling programs with large type graphs that produce deeply nested ASTs (200+ frames deep).

The recursive child traversal is now iterative, using an explicit worklist. This removes the dependency on call stack depth for AST traversal in the reachability pass.

## Add pony-doc tool

We've added an experimental new tool, `pony-doc`, that is intended to replace the `--docs` pass in ponyc. The docs pass will remain in ponyc for now but will eventually be removed.

The most notable difference from the existing `--docs` flag is that `pony-doc` generates documentation for public items only by default. To include private types and methods in the output, use the `--include-private` flag. This replaces the old `--docs-public` flag which worked in reverse — generating everything by default and requiring a flag to restrict to public items only.

Usage:

```bash
pony-doc [options] <package-directory>
```

Options:

- `-o`, `--output`: Output directory (default: current directory)
- `--include-private`: Include private types and methods
- `-V`, `--version`: Print version and exit

## Add Support for Settings to pony-lsp

`pony-lsp` can now be provided with settings from the lsp-client (most of the time this is the editor).
It supports two settings, which are both optional:

- **defines**: A list of strings that will be defined during compilation and can be checked for with `ifdef`. Those defines are usually provided to `ponyc` with the `-D` flag.
- **ponypath**: A string, containing a path or list of paths (like used e.g. in the `$PATH` environment variable) that will be used to tell `pony-lsp` about additional entries to its package search path.

Example settings as JSON:

```json
{
  "defines": ["FOO", "BAR"],
  "ponypath": "/path/to/pony-package:/another/path"
}
```

## Improve pony-lsp diagnostics

`pony-lsp` only relayed the main message from errors provided by ponyc. Now, it also passes the additional information to help explain the error.

## Make pony-lsp version dynamic to match ponyc version

Previously, the version of pony-lsp was hardcoded in the source code. Now it is dynamically generated from the ponyc version.

## Add `\exhaustive\` annotation for match expressions

A new `\exhaustive\` annotation can be applied to match expressions to assert that all cases are explicitly handled. When present, the compiler will emit an error if the match is not exhaustive and no else clause is provided, instead of silently injecting an `else None`.

Without the annotation, a non-exhaustive match silently compiles with an implicit `else None`, which changes the result type to `(T | None)` and produces an indirect type error like "function body isn't the result type." With the annotation, the error message directly identifies the problem.

```pony
type Choices is (T1 | T2 | T3)

primitive Foo
  fun apply(p: Choices): String =>
    match \exhaustive\ p
    | T1 => "t1"
    | T2 => "t2"
    end
```

The above produces: `match marked \exhaustive\ is not exhaustive`

Adding the missing case or an explicit else clause resolves the error. The annotation is also useful on matches that are already exhaustive as a future-proofing measure. Without it, if a new member is later added to the union type and the match isn't updated, the compiler silently injects `else None`. You'll only get an error if the match result is assigned to a variable whose type doesn't include `None` -- otherwise the bug is completely silent. With `\exhaustive\`, the compiler catches the missing case immediately.

## Update Docker image base to Alpine 3.23

The `ponylang/ponyc:nightly` and `ponylang/ponyc:release` Docker images now use Alpine 3.23 as their base image, updated from Alpine 3.21.

