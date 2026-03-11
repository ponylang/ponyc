## Fix pool_memalign crash due to insufficient alignment for AVX instructions

`pool_memalign` used `malloc()` for allocations smaller than 1024 bytes. On x86-64 Linux, `malloc()` only guarantees 16-byte alignment, but the Pony runtime uses SIMD instructions that require stronger alignment (32-byte for AVX, 64-byte for AVX-512). This caused a SIGSEGV on startup for any program built with `-DUSE_POOL_MEMALIGN`. Small allocations now use `posix_memalign()` with 64-byte alignment; large allocations continue to use the full 1024-byte `POOL_ALIGN`.

## Fix `assignment-indent` to require RHS indented relative to assignment

The `style/assignment-indent` lint rule previously only checked that multiline assignment RHS started on the line after the `=`. It did not verify that the RHS was actually indented beyond the assignment line. Code like this was incorrectly accepted:

```pony
    let chunk =
    recover val
      Array[U8]
    end
```

The rule now flags RHS that starts on the next line but is not indented relative to the assignment. The correct form is:

```pony
    let chunk =
      recover val
        Array[U8]
      end
```

## Fix tool install with `use` flags

When building ponyc with `use` flags (e.g., `use=pool_memalign`), `make install` failed because the tools (pony-lsp, pony-lint, pony-doc) were placed in a different output directory than ponyc. Tools now use the same suffixed output directory, and the install target handles missing tools gracefully.

## Fix stack overflow in AST tree checker

The compiler's `--checktree` AST validation pass used deep mutual recursion that could overflow the default 1MB Windows stack when validating programs with deeply nested expression trees. The tree checker has been converted to use an iterative worklist instead.

This is unlikely to affect end users. The `--checktree` flag is a compiler development tool used primarily when building ponyc itself and running the test suite.

## Return memory to the OS when freeing large pool allocations

The pool allocator now returns physical memory to the OS when freeing allocations larger than 1 MB. Previously, freed blocks were kept on the free list with their physical pages committed, meaning the RSS never decreased even after the memory was no longer needed. Now, the page-aligned interior of freed blocks is decommitted via `madvise(MADV_DONTNEED)` on POSIX or `VirtualAlloc(MEM_RESET)` on Windows. The virtual address space is preserved, so pages fault back in transparently on reuse.

This primarily benefits programs that make large temporary allocations (the compiler during compilation, programs with large arrays or strings). Programs whose memory usage is dominated by small allocations (< 1 MB) will see little change — per-size-class caching is a separate mechanism.

To preserve the old behavior (never decommit), build with `make configure use=pool_retain`.

## Fix scheduler stats output for memory usage

Applications compiled against the runtime which prints periodic scheduler statistics will now display the correct memory usage values per scheduler and for messages "in-flight".

## Fix compiler crash when object literal implements enclosing trait

The compiler crashed with an assertion failure when an object literal inside a trait method implemented the same trait. This code would crash the compiler regardless of whether the trait was used:

```pony
trait Printer
  fun double(): Printer =>
    object ref is Printer
      fun apply() => None
    end
  fun apply() => None
```

This is now accepted. The object literal correctly creates an anonymous class that implements the enclosing trait, and inherited methods that reference the object literal work as expected.

## Fix TCPListener accept loop spin on persistent errors

The `pony_os_accept` FFI function returns a signed `int`, but `TCPListener` declared the return type as `U32`. When `accept` returned `-1` to signal a persistent error (e.g., EMFILE — out of file descriptors), the accept loop treated it as "try again" and spun indefinitely, starving other actors of CPU time.

The FFI declaration now correctly uses `I32`, and the accept loop bails out on `-1` instead of retrying. The ASIO event will re-notify the listener when the socket becomes readable, so no connections are lost.

## Update to LLVM 21.1.8

We've updated the LLVM version used to build Pony from 18.1.8 to 21.1.8.

## Compile-time string literal concatenation

The compiler now folds adjacent string literal concatenation at compile time, avoiding runtime allocation and copying. This works across chains that mix literals and variables — adjacent literals are merged while non-literal operands are left as runtime `.add()` calls. For example, `"a" + "b" + x + "c" + "d"` is folded to the equivalent of `"ab".add(x).add("cd")`, reducing four runtime concatenations down to two.

## Exempt unsplittable string literals from line length rule

The `style/line-length` lint rule no longer flags lines where the only reason for exceeding 80 columns is a string literal that contains no spaces. Strings without spaces — URLs, file paths, qualified identifiers — cannot be meaningfully split across lines, so flagging them produced noise with no actionable fix.

Strings that contain spaces are still flagged because they can be split at space boundaries using compile-time string concatenation at zero runtime cost:

```pony
// Before: flagged, and splitting is awkward
let url = "https://github.com/ponylang/ponyc/blob/main/packages/builtin/string.pony"

// After: no longer flagged — the string has no spaces and can't be split

// Strings with spaces can still be split, so they remain flagged:
let msg = "This is a very long error message that should be split across multiple lines"

// Fix by splitting at spaces:
let msg =
  "This is a very long error message that should be split "
  + "across multiple lines"
```

Lines inside triple-quoted strings (docstrings) and lines containing `"""` delimiters are not eligible for this exemption — docstring content should be wrapped regardless of whether it contains spaces.

## Fix compiler crash on `return error`

Previously, writing `return error` in a function body would crash the compiler with an assertion failure instead of producing a diagnostic error. The compiler now correctly reports that a return value cannot be a control statement.

## Add `--sysroot` option

A new `--sysroot` option specifies the target system root. The compiler uses the sysroot to locate libc CRT objects and system libraries for the target platform. For native builds, the host root filesystem is used by default; for cross-compilation, common cross-toolchain locations are searched automatically.

## Use embedded LLD for Linux targets

All Linux builds — both native and cross-compilation — now use the embedded LLD linker directly instead of invoking an external compiler driver via `system()`. For cross-compilation, this eliminates the requirement to have a target-specific GCC cross-compiler installed solely for linking.

The embedded LLD path activates automatically for any Linux target without `--linker` set. To use an external linker instead, pass `--linker=<command>` as an escape hatch to the legacy linking path. The `--link-ldcmd` flag is ignored when using embedded LLD; use `--linker` instead to get legacy behavior.

## Fix compilation not correctly triggered upon startup with pony-lsp

It often happens that, during pony-lsp startup, compilation would be triggered before the necessary settings are being provided (e.g. `ponypath`), so the initial parsing and type-checking of pony-lsp failed. It needed to be retriggered by opening another file or save a currently open file.

This has been fixed by enqueue compilations until settings are provided and only run them if either settings have been provided or we can be sure that no settings can be provided (e.g. if the lsp-client does not implement the necessary protocol bits).

## Native macOS builds now use embedded LLD

macOS linking now uses the embedded LLD linker (Mach-O driver) instead of invoking the system `ld` command. This is part of the ongoing work to eliminate external linker dependencies across all platforms.

If the embedded linker causes issues, use `--linker=ld` to fall back to the system linker.

## Native Windows builds now use embedded LLD

Windows linking now uses the embedded LLD linker (COFF driver) instead of invoking the MSVC `link.exe` command. This is part of the ongoing work to eliminate external linker dependencies across all platforms.

If the embedded linker causes issues, use `--linker=<path-to-link.exe>` to fall back to the system linker.

## Add OpenBSD 7.8 as a tier 3 CI target

OpenBSD is now a tier 3 (best-effort) platform for ponyc. A weekly CI job builds and tests the compiler, standard library, and tools (pony-doc, pony-lint, pony-lsp) on OpenBSD 7.8.

The tools previously couldn't find the standard library on OpenBSD because they hardcoded their binary name when looking up the executable directory. This works on Linux, macOS, and FreeBSD, which have platform APIs that ignore argv0, but fails on OpenBSD where argv0 is the only mechanism for resolving the executable path. All three tools now pass the real `argv[0]` from the runtime.

`libponyc-standalone` is now built on OpenBSD, so programs can link against the compiler as a library on this platform.

## Add safety/exhaustive-match lint rule to pony-lint

pony-lint now flags exhaustive `match` expressions that don't use the `\exhaustive\` annotation. Without `\exhaustive\`, adding a new variant to a union type compiles silently — the compiler injects `else None` for missing cases instead of raising an error. The new `safety/exhaustive-match` rule catches these matches so you can add the annotation and get compile-time protection against incomplete case handling.

The rule is enabled by default. To suppress it for a specific match, use `// pony-lint: allow safety/exhaustive-match` on the line before the match, or disable the entire `safety` category with `--disable safety` or in your config file.

This is the first rule in the new `safety` category. It also introduces `CompileSession` to the `pony_compiler` library, enabling resumable compilation (compile to `PassParse`, inspect the AST, then continue to `PassExpr`).

## Add DragonFly BSD 6.4.2 as a tier 3 CI target

DragonFly BSD is now a tier 3 (best-effort) platform for ponyc. A weekly CI job builds and tests the compiler, standard library, and tools (pony-doc, pony-lint, pony-lsp) on DragonFly BSD 6.4.2.

`libponyc-standalone` is now built on DragonFly BSD, so programs can link against the compiler as a library on this platform. The pony_compiler library and the ffi-libponyc-standalone test previously excluded DragonFly BSD entirely.

## Fix compiler crash in HeapToStack optimization pass on Windows

The compiler could crash during optimization when compiling programs that trigger the HeapToStack pass (which converts heap allocations to stack allocations). The crash was non-deterministic and most commonly observed on Windows, though it could theoretically occur on any platform.

The root cause was that HeapToStack was registered as a function-level optimization pass but performed operations (force-inlining constructors) that are only safe from a call-graph-level pass. This violated LLVM's pass pipeline contract and could cause use-after-free of internal compiler data structures.

## Fix exhaustive match on tuples of union types

When using `match \exhaustive\` on a tuple of union types, the compiler rejected matches that used concrete types for the final case even when they covered all remaining possibilities. For example, this was incorrectly rejected:

```pony
type A is String

actor Main
  new create(env: Env) =>
    let x: (A | None) = None
    let y: (A | None) = None

    match \exhaustive\ (x, y)
    | (let a: A, _) => env.out.print("1")
    | (_, let a: A) => env.out.print("2")
    | (None, None) => env.out.print("3")
    end
```

The compiler reported `match marked \exhaustive\ is not exhaustive`, even though the first two cases cover every combination where at least one element is `A`, and the third case covers the only remaining possibility `(None, None)`. Replacing `(None, None)` with `(_, _)` was accepted as a workaround.

The underlying issue was in the subtype checker: a tuple of unions like `((A | None), (A | None))` was not recognized as a subtype of the equivalent union of tuples. This is now fixed by distributing tuples over unions when checking subtype relationships.

## Add json and iregex packages to the standard library

Two new packages have been added to the standard library per RFC 0081.

The `json` package provides immutable JSON values backed by persistent collections. It includes `JsonParser` for parsing, `JsonNav` for chained read access, `JsonLens` for composable get/set/remove paths, and `JsonPath` for RFC 9535 string-based queries with filters and function extensions.

```pony
use json = "json"

// Build
let doc = json.JsonObject
  .update("name", "Alice")
  .update("age", I64(30))

// Parse
match json.JsonParser.parse(source)
| let j: json.JsonValue => // ...
| let err: json.JsonParseError => // ...
end

// Navigate
let nav = json.JsonNav(doc)
try nav("name").as_string()? end

// Query
try
  let path = json.JsonPathParser.compile("$.users[?@.age > 21]")?
  let results = path.query(doc)
end
```

The `iregex` package provides I-Regexp (RFC 9485) pattern matching — a deliberately constrained regular expression syntax designed for interoperability. The `json` package uses it internally for JSONPath `match()` and `search()` filter functions, but it is also available as a standalone package.

```pony
use iregex = "iregex"

match iregex.IRegexpCompiler.parse("[a-z]+")
| let re: iregex.IRegexp =>
  re.is_match("hello")   // true — full-string match
  re.search("abc123def") // true — substring match
| let err: iregex.IRegexpParseError =>
  // handle error
  None
end
```

