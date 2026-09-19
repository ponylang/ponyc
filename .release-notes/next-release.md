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

## Add Generators.f32 and Generators.f64 to PonyCheck

PonyCheck now has built-in `F32` and `F64` generators. Both accept `from` and `to` parameters (defaulting to `0.0` and `1.0`) and normalize argument order, matching the integer generator API.

```pony
// Generate F64 values in [0.0, 1.0] (the default)
let gen = Generators.f64()

// Generate F32 values in [-100.0, 100.0]
let gen = Generators.f32(where from = -100.0, to = 100.0)
```

The generators work across the full type range, including `Generators.f64(where from = -F64.max_value(), to = F64.max_value())`. They error on NaN inputs and shrink generated values toward zero (or the nearest bound) automatically.

Previously, floating-point generation required `Generators.repeatedly` with a lambda, which produced values that could not be shrunk.

## Add classification API to PonyCheck

Property-based tests can now report how their generated inputs distribute across categories. We added four methods to `PropertyHelper` following the classify/tabulate/cover pattern from QuickCheck and Hypothesis:

```pony
use "pony_check"

class iso MyProperty is Property1[U8]
  fun name(): String => "my property"

  fun gen(): Generator[U8] => Generators.u8(0, 100)

  fun ref property(sample: U8, h: PropertyHelper) =>
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

