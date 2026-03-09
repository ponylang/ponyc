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

