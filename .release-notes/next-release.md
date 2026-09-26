## Bound PonyTest concurrent execution to scheduler thread count

PonyTest previously launched all non-exclusive tests at once with no concurrency limit. In large test suites this overwhelmed the scheduler, causing spurious timeouts in tests that pass when run on their own.

Concurrent test execution is now bounded by the number of scheduler threads. Tests beyond that limit are queued and started as earlier tests complete. Small test suites are unaffected — when there are fewer tests than threads, all tests still start immediately.

## Add `\by_value\` FFI annotation for struct-by-value passing

Pony's FFI passes all structs behind pointers. Calling a C function that takes or returns a struct by value previously required writing a C wrapper by hand. The `\by_value\` annotation eliminates that — the compiler generates the wrapper for you.

Annotate each parameter or return type that the C function expects by value:

```pony
struct Point
  var x: F64 = 0
  var y: F64 = 0

use @point_add[Point \by_value\](a: Point \by_value\, b: Point \by_value\)
```

Struct fields must be primitives or pointers. Nested embedded structs, zero-field structs, 128-bit integers, and variadic functions are not supported.

## Add AsioBackend trait for mockable ASIO operations

`TCPConnection`, `TCPListener`, and the related traits and type aliases now take a second type parameter `Asio: AsioBackend ref = RuntimeAsio` that controls which ASIO backend the connection uses. The default `RuntimeAsio` delegates to the runtime's `PonyAsio` calls, so existing code that omits the parameter compiles without changes.

The new `AsioBackend` trait covers the full `PonyAsio` operation surface. `RuntimeAsio` is the production implementation.

This parallels the existing `TCPBackend` / `RuntimeBackend` seam: where `TCPBackend` lets tests replace socket I/O, `AsioBackend` lets tests replace timer creation, event subscription, and readability/writeability signaling. Together the two seams make it possible to test connection logic — including idle-timer resets on send and receive — without real sockets or real timers.

To supply a custom ASIO backend:

```pony
actor MyServer is (TCPConnectionActor[MyBackend, MyAsio]
  & ServerLifecycleEventReceiver[MyBackend, MyAsio])

  var _conn: TCPConnection[MyBackend, MyAsio] =
    TCPConnection[MyBackend, MyAsio].none()

  fun ref _connection(): TCPConnection[MyBackend, MyAsio] => _conn
```

## Add `--lib-path` / `-L` flag for extra linker library search paths

The new `--lib-path` flag (short form `-L`) adds a library search directory that the linker checks before its auto-discovered paths. It can be specified multiple times.

Unlike `--path`, which adds directories to both Pony package resolution and the linker, `--lib-path` affects only the linker. This is useful when cross-compiling and the target-architecture libraries live outside the sysroot — for example, Debian multiarch packages installed to `/usr/lib/<triple>/`:

```
ponyc --triple riscv64-linux-gnu --lib-path /usr/lib/riscv64-linux-gnu mypackage
```

## Fix cycle detector memory leak for mutually-referencing actors

Long-running programs that repeatedly create and destroy pairs of actors holding references to each other leaked memory proportional to the number of pairs destroyed. A daemon accepting connections where each connection actor creates a handler that references it back leaked roughly 350 bytes per connection, with no ceiling.

Found and diagnosed by @In2infinity, who also provided a working fix that guided this change.

## Add `is_socket_connected` to `TCPBackend` trait

`TCPBackend` now has an `is_socket_connected(fd: U32): Bool` method. The production `RuntimeBackend` checks `SO_ERROR` on the socket. Mock backends can return `true` or `false` to control the connect outcome without a real socket.

## `TCPBackend` implementations must add `is_socket_connected`

`TCPBackend` has a new required method: `is_socket_connected(fd: U32): Bool`. Existing implementations must add it.

Before:

```pony
class MyBackend is TCPBackend
  // ... existing methods
```

After:

```pony
class MyBackend is TCPBackend
  fun is_socket_connected(fd: U32): Bool => false
  // ... existing methods
```

## Add Generators.f32 and Generators.f64 to PonyTest

PonyTest now has built-in `F32` and `F64` generators. Both accept `from` and `to` parameters (defaulting to `0.0` and `1.0`) and normalize argument order, matching the integer generator API.

```pony
// Generate F64 values in [0.0, 1.0] (the default)
let gen = Generators.f64()

// Generate F32 values in [-100.0, 100.0]
let gen = Generators.f32(where from = -100.0, to = 100.0)
```

The generators work across the full type range, including `Generators.f64(where from = -F64.max_value(), to = F64.max_value())`. They error on NaN inputs and shrink generated values toward zero (or the nearest bound) automatically.

Previously, floating-point generation required `Generators.repeatedly` with a lambda, which produced values that could not be shrunk.

## Add classification API to PonyTest

Property-based tests can now report how their generated inputs distribute across categories. We added four methods to `TestHelper` following the classify/tabulate/cover pattern from QuickCheck and Hypothesis:

```pony
use "pony_test"

class iso MyProperty is Property[U8]
  fun name(): String => "my property"

  fun gen(): Generator[U8] => Generators.u8(0, 100)

  fun ref property(sample: U8, h: TestHelper) =>
    // flat label — reported as a percentage of all samples
    h.classify(if sample < 10 then "small" else "large" end)

    // grouped label — independent counter under a named heading
    h.tabulate("parity",
      if (sample %% 2) == 0 then "even" else "odd" end)

    // coverage requirement — fails the property without shrinking
    // when fewer than 5% of samples carry the label
    h.cover(sample < 10, "small", 5.0)

    // convenience — classifies using value.string()
    h.collect(sample)
```

After all samples run, the runner prints a distribution table. A `ClassificationNotify` callback gives programmatic access to the flat and tabulated counts.

## Fix out-of-memory crash on ILP32 Linux from address space fragmentation

On 32-bit Linux (ILP32), compiling large programs could crash with "out of memory" well before hitting the physical memory limit. The pool arena's aligned memory allocation mapped twice the requested size to find an aligned boundary, then unmapped the excess. Each 64MB region consumed 128MB of address space, fragmenting the 3GB user space so badly that allocation failed at roughly 1.5GB of actual use.

The allocator now uses `MAP_FIXED_NOREPLACE` (available since Linux 4.17, within ponyc's existing 5.3 kernel minimum) to request aligned addresses directly, avoiding the overallocation. It falls back to the old approach when the aligned probes fail.

## Add crypto package to the standard library

The `crypto` package provides cryptographic primitives backed by OpenSSL's libcrypto.

One-shot hash functions cover the common case where all the data is available at once:

```pony
use "crypto"

let hash = SHA256("Hello, World!")
env.out.print(ToHexString(hash))
```

Available one-shot functions: `MD4`, `MD5`, `RIPEMD160`, `SHA1`, `SHA224`, `SHA256`, `SHA384`, `SHA512`.

The streaming `Digest` class hashes data that arrives in pieces:

```pony
let d = Digest.sha256()?
d.append("Hello, ")?
d.append("World!")?
let hash = d.final()?
```

On OpenSSL 3.0.x and 4.0.x, `Digest.shake128` and `Digest.shake256` produce variable-length output.

The package also includes `HmacSha256` for message authentication, `Pbkdf2Sha256` for key derivation, `RandBytes` for cryptographically secure random bytes, and `ConstantTimeCompare` for timing-safe comparison.

If your code depended on `ponylang/ssl` for crypto, switch to `use "crypto"` with no code changes beyond the import path.

## Fix unnecessary per-byte allocation in ToHexString

`ToHexString` allocated a temporary string for every input byte. Converting a SHA-512 hash to hex produced 64 intermediate strings. The conversion now runs with a single allocation for the output.

## ConstantTimeCompare now uses OpenSSL's CRYPTO_memcmp

LLVM may replace the pure-Pony XOR-accumulate loop in `ConstantTimeCompare` with an early exit once the accumulator is non-zero. That transformation preserves the return value but destroys the constant-time property. Pony has no `volatile` or compiler barrier to prevent it.

`ConstantTimeCompare` now calls OpenSSL's `CRYPTO_memcmp`, which is guaranteed constant-time. The `crypto` package already links libcrypto, so no new dependency is needed.

## Fix Reader crash when appending empty data

Calling `Reader.append` with an empty `Array[U8]` or empty `String` pushed a zero-length chunk into the internal chunk list. A subsequent read — even with sufficient data from other appends — would error when trying to index into the empty chunk.

## Add stateful property testing to PonyTest

PonyTest now supports stateful property testing, where each sample creates a system under test and a reference model, then runs a sequence of randomly generated steps that operate on both. After each step, an invariant checks that the model and the SUT agree. On failure, the choice sequence is shrunk to find a minimal reproducing case.

```pony
use "pony_test"

class iso _CounterProperty
  is StatefulProperty[_Counter, USize, _Increment]
  fun name(): String => "counter"
  fun max_steps(): USize => 20
  fun initial_sut(): _Counter => _Counter
  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[_Counter, USize],
    rnd: Randomness,
    h: TestHelper)
    : _Increment
  =>
    ctx.sut.increment()
    ctx.model = ctx.model + 1
    _Increment

  fun invariant(
    ctx: StatefulContext[_Counter, USize] box,
    h: TestHelper)
    : Bool
  =>
    h.assert_eq[USize](ctx.model, ctx.sut.count)
```

Register with PonyTest using the `StatefulPropertyTest` adapter:

```pony
test(StatefulPropertyTest[_Counter, USize, _Increment](
  _CounterProperty))
```

The `step` method draws randomness, applies a command to both the SUT and the model, and returns a `Stringable val` command object used in failure reporting. When no valid command exists in the current state, `step` errors and the runner retries with a fresh sample.

## Improve error message when writing to a field in an immutable method

Writing to a field inside a `fun` (which defaults to `box` receiver capability) used to produce the generic error "left side is immutable." The compiler now reports "cannot write to a field in a box function. If you are trying to change state in a function use fun ref."

## Fix error message for nested field writes in immutable methods

Writing to a field through another field — `my_object.x = 1` where `my_object` is itself a field on `this` — inside a `fun box`, `fun val`, or `fun tag` method produced the generic "left side is immutable" error. The compiler now reports "cannot write to a field in a box function. If you are trying to change state in a function use fun ref," matching the message already given for direct field writes like `x = 42`.

## Fix File losing data on large writes

Writing a large amount of data to a `File` could silently lose the entire write and close the file. When the operating system wrote only part of the data in a single call (a short write), `File` raised an error, closed the file, and discarded all pending data, including the bytes already written.

Short writes are normal when the data exceeds the OS buffer size. `File` now retries after a short write, advancing past the bytes already written, until all data reaches the file or a real I/O error occurs.

## Improve error messages for method lookup on anonymous types

When calling a nonexistent method on a lambda, object literal, or partial application, the error message showed the compiler's internal name for the type (e.g., `$1$0`) instead of something readable. The error now says "anonymous type" and lists the methods available on it, so you can see what you can actually call.

Before:

```
couldn't find 'string' in '$1$0'
```

After:

```
couldn't find 'string' in anonymous type
    it has a method named 'apply'
```

## Add health check warnings to PonyTest

PonyTest now logs diagnostic warnings after a property run completes when it detects patterns that often signal a problem with the generator or property: a high filter rejection rate, an unusually large choice sequence, or a slow sample. The warnings never cause test failure — they appear in the test log when running with `--verbose`.

Three new fields on `PropertyParams` control the thresholds. All three default to values that avoid false positives on typical properties. Setting any threshold to 0 disables that check.

```pony
fun params(): PropertyParams =>
  PropertyParams(where
    max_filter_discard_ratio' = 5.0,
    max_choice_sequence_size' = 500,
    max_sample_nanos' = 500_000_000)
```

## `HashSet` no longer provides `is Comparable`

`HashSet` now implements `Equatable` instead of `Comparable`. The comparison operators (`<`, `<=`, `>`, `>=`, `==`, `!=`) are defined directly on `HashSet` and continue to work. Only the inherited `compare` method is removed.

`HashSet`'s `lt` defines a strict-subset relation, which is a partial order. `Comparable`'s default `compare` assumes a total order, so it returned `Greater` for two disjoint sets in both directions.

Code that calls `.compare()` on a `HashSet` or passes one where `Comparable[HashSet[...]]` is required needs updating:

Before:

```pony
let ordering = set1.compare(set2)
```

After:

```pony
if set1 == set2 then
  // equal
elseif set1 < set2 then
  // strict subset
end
```

## persistent `HashSet` no longer provides `is Comparable`

The persistent `HashSet` in `collections/persistent` now implements `Equatable` instead of `Comparable`. The comparison operators continue to work. Only the inherited `compare` method is removed.

The same partial-order issue applies: `lt` defines strict subset, and `Comparable`'s default `compare` returned `Greater` for disjoint sets in both directions.

Before:

```pony
use "collections/persistent"

let ordering = set1.compare(set2)
```

After:

```pony
use "collections/persistent"

if set1 == set2 then
  // equal
elseif set1 < set2 then
  // strict subset
end
```

## `Flags` no longer provides `is Comparable`

`Flags` now implements `Equatable` instead of `Comparable`. The comparison operators (`<`, `<=`, `>`, `>=`, `==`, `!=`) are defined directly on `Flags` and continue to work. Only the inherited `compare` method is removed.

`Flags`'s `lt` defines a strict-subset relation on the set of enabled flags, which is a partial order. `Comparable`'s default `compare` returned `Greater` for two flag values with disjoint bits in both directions.

Code that calls `.compare()` on a `Flags` value (including `FileCaps`) or passes one where `Comparable[Flags[...]]` is required needs updating:

Before:

```pony
let ordering = my_flags.compare(other_flags)
```

After:

```pony
if my_flags == other_flags then
  // same flags set
elseif my_flags < other_flags then
  // strict subset
end
```

## Fix out-of-memory when compiling large programs on 32-bit platforms

Compiling large programs on 32-bit platforms (such as RPi4 in 32-bit mode) could exhaust address space because the compiler held the entire program in a single LLVM module. Peak memory is now proportional to the largest package rather than the whole program. The stdlib compiles on RPi4 with ponyc peaking at 2918MB and the linker at 2060MB, both under the ~3GB ILP32 limit.

## Fix debug info generation so release builds no longer need to strip DWARF data

The compiler's DWARF generation had bugs that caused crashes or invalid debug info in optimized builds. The old workaround auto-set `--strip` on every release build, discarding all debug info. The underlying DWARF issues are fixed in the new per-package codegen pipeline, so the forced stripping is removed. Release builds now produce valid debug info by default. Pass `--strip` explicitly if you want binaries without debug info.

## `--print_stats` no longer reports heap-to-stack promotion counts

The `heap_alloc` and `stack_alloc` counters are removed from `--print_stats` output. HeapToStack now runs during LTO linking rather than inside the compiler, so the compiler has no access to the promotion counts. A debug build of ponyc logs each promotion decision to stderr via LLVM's debug output (`-debug-only=pony-heap-to-stack`) and tracks aggregate counts via LLVM's `STATISTIC` mechanism (`NumHeapAlloc`, `NumStackAlloc`).

## Release builds no longer strip debug info automatically

Release builds previously auto-set `--strip`, removing all debug info from the binary. That was a workaround for bugs in DWARF generation, not intentional policy. Those bugs are fixed, so the forced stripping is removed. Binaries built with `--release` now include debug info unless `--strip` is passed explicitly. If your build scripts or packaging rely on release binaries having no debug info, add `--strip` to your ponyc invocation.

## Remove `--extfun` flag

The `--extfun` flag forced external linkage on all functions, which was useful for making release-build symbols visible to debuggers and profilers. All symbols now start with external linkage so the LTO linker can determine final visibility, making `--extfun` redundant. Passing it is no longer accepted.

## Replace optimize-then-emit pipeline with per-package codegen

The compiler now optimizes and generates code one package at a time instead of processing the entire program at once. Peak memory during compilation is proportional to the largest package rather than the whole program. This makes it possible to compile large programs on memory-constrained targets like 32-bit ARM (RPi4) that previously ran out of memory.

Two LTO modes control how the linker combines the per-package output:

- `--fat-lto` (default): full LTO merges everything into one unit before optimizing. Best optimization, higher memory use during linking.
- `--thin-lto`: ThinLTO optimizes each package individually with cross-module information. Faster linking, lower memory use, slightly less optimization.

`--pass ir` emits per-package LLVM IR. `--pass bitcode` emits per-package bitcode files.

## Remove `--pass asm` and `--pass obj`

The compiler no longer drives the LLVM backend directly — the linker handles optimization and native codegen during LTO linking. `--pass asm` and `--pass obj` have no equivalent in this pipeline.

## Fix runtime bitcode performance

Programs compiled with runtime bitcode ran up to 2x slower than programs compiled without it. The runtime bitcode was compiled with no optimization flags, preventing the optimizer from inlining runtime functions into user code. It is now compiled at `-O2`.

## Inline `pony_alloc` and `pony_alloc_small` calls that HeapToStack does not promote

Runtime allocation calls that could not be promoted to the stack were left as function calls. A new LTO pass now inlines these remaining call sites after heap-to-stack promotion, eliminating the call overhead. In message-heavy workloads this produces faster binaries than linking the native runtime library.

## Remove DTrace and SystemTap support

DTrace and SystemTap USDT probes are no longer available. The `use=dtrace` build option has been removed.

The probes were incompatible with compiling the runtime as bitcode, and they were only available on a subset of supported platforms (macOS, Linux, FreeBSD). The runtime's built-in tracing system (`runtime_info`, flight recorder) is not affected.
## Make runtime bitcode the default

On clang-based platforms (Linux, macOS), ponyc now optimises across the boundary between your code and the runtime by default. Binaries are smaller and faster with no flag required. On Windows/MSVC, where bitcode is not available, linking behaviour is unchanged.

The `--runtimebc` CLI flag has been removed. If your build scripts pass `--runtimebc`, remove it — the behaviour is now automatic.

On macOS, shared libraries that call PONY_API runtime functions (`pony_exitcode`, `pony_alloc`, etc.) must no longer link `libponyrt`. The runtime now lives in the executable, and linking `libponyrt` into a shared library creates a second copy of runtime state. Instead, build the shared library with `-undefined dynamic_lookup` so its runtime calls resolve from the executable at load time. On Linux and FreeBSD no change is needed — ELF's flat symbol namespace resolves from the executable by default.

## Export PONY_API symbols on ELF platforms when runtime bitcode is merged

On ELF platforms (Linux, FreeBSD, DragonFly, OpenBSD), the linker flag `--export-dynamic` was only passed in debug builds of ponyc. With runtime bitcode now merged into the executable by default, LTO can internalize PONY_API functions that nothing in the program calls directly. A shared library loaded at runtime via FFI that calls PONY_API functions would fail to resolve them.

The ELF linker now passes `--export-dynamic` whenever runtime bitcode is merged, matching the macOS linker path.

## Fix optimizer attributes lost after runtime linking

After the runtime was linked into the program, LLVM attributes on runtime functions were lost. Without them, unnecessary unwind paths remained at every runtime call site and memory operations could not be moved past allocator calls. The attributes are now preserved across linking.

## Add inacc_or_arg_mem to pony_send_done and pony_recv_done

`pony_send_done` and `pony_recv_done` now carry the `inacc_or_arg_mem` memory attribute, matching the trace functions they call internally. This lets LLVM hoist and sink loads across GC completion calls, improving optimization of code around message sends and receives.

## Add regression persistence to PonyTest

When a property test fails, the shrunk failing choice sequence is saved to disk. On the next run, the stored sequence is replayed before random samples are generated. If it still fails, the property fails immediately with the same minimal case. If it passes, the stored regression is cleared and all configured random samples run normally.

Both `Property` and `StatefulProperty` support regression persistence. Regressions are stored in a `.ponytest/` directory under the working directory, one file per property.

Two environment variables control the behavior:

- `PONYTEST_NO_DB=1` disables persistence entirely.
- `PONYTEST_DB_DIR=path` changes the storage directory.

Individual properties can opt out via `PropertyParams`:

```pony
fun params(): PropertyParams =>
  PropertyParams(where regression_db' = false)
```

Persistence is off for properties registered through the `ForAll` convenience API.

## Fix String.copy_cpointer reading one byte past the source buffer

`String.copy_cpointer` copied `len + 1` bytes from the source pointer, assuming a null terminator existed at position `len`. The method's contract is to copy a fixed number of bytes — not a C string — so any source buffer without a trailing null was overread by one byte. This affected FFI callbacks that receive length-delimited buffers, such as the OpenSSL ALPN select callback.

## Merge property testing into PonyTest

The `pony_check` package is removed. All property testing types and functions are now in `pony_test`.

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
  fun ref property(sample: U8, h: TestHelper) ? =>
    h.assert_true(sample < 200)
```

Full migration list:

- `use "pony_check"` → `use "pony_test"`
- `Property1[T]` → `Property[T]`
- `PropertyHelper` → `TestHelper`
- `Property1UnitTest[T]` → `PropertyTest[T]`
- `StatefulPropertyUnitTest[S, M, Cmd]` → `StatefulPropertyTest[S, M, Cmd]`
- `PONYCHECK_NO_DB` → `PONYTEST_NO_DB`
- `PONYCHECK_DB_DIR` → `PONYTEST_DB_DIR`
- `.ponycheck/` regression directory → `.ponytest/`

`Property2`, `Property3`, `Property4`, `Generators`, `Randomness`, `StatefulProperty`, `StatefulContext`, `ForAll`, and `ClassificationNotify` are unchanged.

`PropertyParams` drops the `async` field. Remove `async' = true` from any `PropertyParams` constructor call. Async completion now works automatically through `TestHelper.complete` / `expect_action` / `complete_action`.

## Fix property test failures not triggering shrinking

When a property function raised an error or a stateful property's invariant or final_check returned false, the runner did not detect the failure synchronously. It checked a field that could only be set by asynchronous behaviours, so synchronous failures were treated as passes. Failing properties now trigger shrinking and regression persistence.

