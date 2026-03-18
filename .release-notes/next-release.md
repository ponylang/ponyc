## Fix incorrect CLOCK_MONOTONIC value on OpenBSD

On OpenBSD, `CLOCK_MONOTONIC` is defined as `3`, not `4` like on FreeBSD and DragonFly BSD. Pony's `time` package was using `4` for all BSDs, which meant that on OpenBSD, every call to `Time.nanos()`, `Time.millis()`, and `Time.micros()` was actually reading `CLOCK_THREAD_CPUTIME_ID` — the CPU time consumed by the calling thread — instead of monotonic wall-clock time.

The practical result is that the `Timers` system, which relies on `Time.nanos()` for scheduling, would misfire on OpenBSD. Timers set for wall-clock durations would instead fire based on how much CPU time the scheduler thread had burned. For a mostly-idle program, a 60-second timer could take far longer than 60 seconds to fire. Any other code that depends on monotonic time would see similarly wrong values. Hilarity ensues.

## Don't allow --path to override standard library

Previously, directories passed via `--path` on the ponyc command line were searched before the standard library when resolving package names. This meant a `--path` directory could contain a subdirectory that shadows a stdlib package. For example, `--path /usr/lib` would cause `/usr/lib/debug/` to shadow the stdlib `debug` package, breaking any code that depends on it.

The standard library is now always searched first, before any `--path` entries. This is the same fix that was applied to `PONYPATH` in [#3779](https://github.com/ponylang/ponyc/issues/3779).

If you were relying on `--path` to override a standard library package, that will no longer work.

## Fix illegal instruction crashes in pony-lint, pony-lsp, and pony-doc

The tool build commands didn't pass `--cpu` to ponyc, so ponyc targeted the build machine's specific CPU via `LLVMGetHostCPUName()`. Release builds happen on CI machines that often have newer CPUs than user machines. When a user ran pony-lint, pony-lsp, or pony-doc on an older CPU, the binary could contain instructions their CPU doesn't support, resulting in a SIGILL crash.

The C/C++ side of the build already respected `PONY_ARCH` (e.g., `arch=x86-64` targets the baseline x86-64 instruction set). The Pony code generation for the tools just wasn't wired up to use it. Now it is.

## Fix failure to build pony tools with Homebrew

When building ponyc from source with Homebrew, the self-hosted tools (pony-lint, pony-lsp, pony-doc) failed to link because the embedded LLD linker couldn't find zlib. Homebrew installs zlib outside the default system linker search paths, and while CMake resolved the correct location for linking ponyc itself, that path wasn't forwarded to the ponyc invocations that compile the tools.

## Fix memory leak

When a behavior was called through a trait reference and the concrete actor's parameter had a different trace-significant capability (e.g., the trait declared `iso` but the actor declared `val`), the ORCA garbage collector's reference counting was broken. The sender traced the parameter with one trace kind and the receiver traced with another, causing field reference counts to never reach zero. Objects reachable from the parameter were leaked.

```pony
trait tag Receiver
  be receive(b: SomeClass iso)

actor MyActor is Receiver
  be receive(b: SomeClass val) =>
    // b's fields were leaked when called through a Receiver reference
    None
```

The leak is not currently active because `make_might_reference_actor` — an optimization that was disabled as a safety net — masks it by forcing full tracing of all immutable objects. Without this fix, the leak would have become active as we started re-enabling that optimization.

This applies to simple nominal parameters as well as compound types — tuples, unions, and intersections — where the elements have different capabilities between the trait and concrete method.

The sender and receiver now use consistent tracing for each parameter, regardless of capability differences between the trait and concrete method.

