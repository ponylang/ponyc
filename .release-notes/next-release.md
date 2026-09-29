## Update to LLVM 23.1.2

We've updated the LLVM version used to build Pony from 22.1.6 to 23.1.2.

## Runtime tracing flags available in all builds

The runtime tracing flags (`--tracecategories`, `--flightrecorder`, `--ponytracingmode`, etc.) are now available in every ponyc build. Previously they required building ponyc with the `runtime_tracing` option enabled. When no tracing flags are passed, the runtime checks a single boolean at each trace point and skips the call — benchmarks show the overhead is not measurable.

## Remove `runtime_tracing` build option

Runtime tracing is now always compiled in. The `runtime_tracing` entry in `PONY_USES` is no longer accepted — drop it from your CMake configuration.

The `pony_type_t` struct layout changed: `name` and `get_behavior_name` fields are now always present. Code that constructs `pony_type_t` values directly in C must be recompiled.

## Reject runtime-reserved signals in HandleableSignalValidator

`HandleableSignalValidator` now rejects signals the runtime reserves for its tracing thread-pause mechanism. On BSD and macOS, `Sig.info()` (SIGINFO) is rejected. On Linux, real-time signals 32 through 35 are rejected — the C library and the runtime reserve them.

Programs that registered a `SignalHandler` for one of these signals will now get a `ValidationFailure` from `MakeHandleableSignal` instead of silently replacing the runtime's handler.

## Fix ARM64 Windows LTO crash

Building ponyc from source on ARM64 Windows crashed during LTO linking with an access violation in LLVM's type legalizer. MSVC generates incorrect ARM64 machine code for functions in this pass. The vendored LLVM libraries are now built with clang-cl on this platform.

## Build vendored LLVM libraries with clang-cl on ARM64 Windows

The vendored LLVM libraries on ARM64 Windows are now built with clang-cl instead of MSVC. If you build ponyc from source on ARM64 Windows, install the "C++ Clang Compiler for Windows" component in your Visual Studio installation — it is not part of the default "Desktop Development with C++" workload.

## Fix compiling with runtime tracing on the BSDs

Compiling with runtime tracing on the BSDs failed because no one had ever tried before and noticed that it didn't work. FreeBSD, OpenBSD and Dragonfly have all been fixed.

## Fix compiling with runtime tracing on RISC-V

Compiling with runtime tracing on RISC-V failed because no one had ever tried before and noticed that it didn't work.

## Merge property testing into PonyTest

The `pony_check` package has been removed. All property testing types and functions are now in `pony_test`. Change `use "pony_check"` to `use "pony_test"`.

Before:

```pony
use "pony_check"

class iso MyProperty is Property1[U8]
  fun name(): String => "my property"
  fun gen(): Generator[U8] => Generators.u8()
  fun ref property(sample: U8, h: PropertyHelper) ? =>
    h.assert_true(sample < 200)
```

After:

```pony
use "pony_test"

class iso MyProperty is Property[U8]
  fun name(): String => "my property"
  fun gen(): Generator[U8] => Generators.u8()
  fun ref property(sample: U8, h: PropertyHelper) ? =>
    h.assert_true(sample < 200)
```

`Property2`, `Property3`, `Property4`, `PropertyHelper`, `Generators`, `Randomness`, `StatefulProperty`, `StatefulContext`, and `ForAll` are unchanged. Internal types that were previously public, including `PropertyRunner` and `StatefulPropertyRunner`, are now private to `pony_test`.

### Property test registration API changes

Register property tests through methods on `TestList` instead of wrapper classes:

- `test(Property1UnitTest[T](prop))` → `test.property(prop)`
- `test(Property2UnitTest[T1, T2](prop))` → `test.property[(T1, T2)](prop)`
- `test(Property3UnitTest[T1, T2, T3](prop))` → `test.property[(T1, T2, T3)](prop)`
- `test(Property4UnitTest[T1, T2, T3, T4](prop))` → `test.property[(T1, T2, T3, T4)](prop)`
- `test(IntUnitTest(prop))` → `test.property[IntPropertySample](prop)`
- `test(IntPairUnitTest(prop))` → `test.property[IntPairPropertySample](prop)`
- `test(StatefulPropertyUnitTest[S, M, Cmd](prop))` → `test.stateful_property(prop)`

`Property2`/`Property3`/`Property4`, `IntProperty`, and `IntPairProperty` subclasses need explicit type arguments on `test.property()` because the sample type is not inferred through the intermediate trait. Classes that implement `Property[T]` directly don't need type arguments.

### Inline property tests use TestHelper

The `for_all` functions are now on `TestHelper` instead of the `PonyCheck` primitive:

- `PonyCheck.for_all[T](gen, h)` → `h.for_all[T](gen)`
- `PonyCheck.for_all2[T1, T2](gen1, gen2, h)` → `h.for_all2[T1, T2](gen1, gen2)`
- `PonyCheck.for_all3[T1, T2, T3](gen1, gen2, gen3, h)` → `h.for_all3[T1, T2, T3](gen1, gen2, gen3)`
- `PonyCheck.for_all4[T1, T2, T3, T4](gen1, gen2, gen3, gen4, h)` → `h.for_all4[T1, T2, T3, T4](gen1, gen2, gen3, gen4)`

### Async property tests now use long_test()

Properties that do async work — spawning actors, setting up TCP connections, using timers — must call `h.long_test(timeout)` in their `property()` body before any `h.expect_action()` calls. Without it, the property runs synchronously and the next sample starts before callbacks arrive, so the property passes without verifying anything. This is the same contract that regular `UnitTest`s use.

The `async` field on `PropertyParams` has been removed. Remove `async' = true` from any `PropertyParams` constructor call.

Before:

```pony
class iso MyAsyncProperty is Property[String]
  fun params(): PropertyParams =>
    PropertyParams(where async' = true, timeout' = 5_000_000_000)

  fun ref property(sample: String, ph: PropertyHelper) =>
    ph.expect_action("verify")
    SomeActor(ph, sample)
```

After:

```pony
class iso MyAsyncProperty is Property[String]
  fun params(): PropertyParams =>
    PropertyParams(where timeout' = 5_000_000_000)

  fun ref property(sample: String, ph: PropertyHelper) =>
    ph.long_test(params().timeout)
    ph.expect_action("verify")
    SomeActor(ph, sample)
```

Properties that don't call `long_test` run in sync mode, which is faster.

### Property test regression directory and environment variables

- `PONYCHECK_NO_DB` → `PONYTEST_NO_DB`
- `PONYCHECK_DB_DIR` → `PONYTEST_DB_DIR`
- `.ponycheck/` regression directory → `.ponytest/`. If you rename `.ponycheck/` to `.ponytest/`, old files in it are deleted when tests first run because the format is incompatible. Regressions must be regenerated.

## Fix property test failures not triggering shrinking

Property tests that failed by raising an error, or whose invariant or `final_check` returned `false`, did not shrink to a minimal failing case or persist regressions. They now do.
