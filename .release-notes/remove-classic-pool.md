## Remove the classic pool allocator

The `pool_classic`, `pool_retain`, and `pooltrack` build options are removed. The arena allocator, the default since 0.69.0, is now the only full allocator. `pool_memalign` remains for builds that need AddressSanitizer or Valgrind.

Builds that used `pool_classic,valgrind` should switch to `pool_memalign,valgrind`:

Before:

```bash
cmake --preset debug -DPONY_USES=pool_classic,valgrind
```

After:

```bash
cmake --preset debug -DPONY_USES=pool_memalign,valgrind
```

Builds that used `pool_classic` on its own should drop it — the default arena allocator is what runs.
