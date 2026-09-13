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

