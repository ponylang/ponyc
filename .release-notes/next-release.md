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

