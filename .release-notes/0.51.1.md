## Fix String.f32 and String.f64 errors with non null terminated strings

Until now, the `String.f32` and `String.f64` methods required null-terminated strings in order to work properly. This wasn't documented, and wasn't the intended behaviour. We've now fixed these cases and these functions should work as expected for non-null-terminated strings.

## Fix for infinite Ranges

Even though all Range objects that contain NaN parameters or a zero step parameter are considered infinite, some did not iterate at all or produced an error after the first iteration. For example, the code from the Range documentation: 

```pony
  // this Range will produce 0 at first, then infinitely NaN
  let nan: F64 = F64(0) / F64(0)
  for what_am_i in Range[F64](0, 1000, nan) do
    wild_guess(what_am_i)
  end 
```

did not run as expected, but rather produced an error on the first iteration. This is now fixed, and .next() calls on the above `Range[F64](0, 1000, nan)` now first yields 0 and subsequently indefinetely NaN values. Likewise, `Range(10, 10, 0)` will now indefinitely yield `10`.

## Update to basing musl images off of Alpine 3.12

We supply nightly and release Docker images for ponyc based on Alpine Linux. We've updated the version of Alpine we use from 3.12 which recently reached it's end of life to Alpine 3.16 which is supported until 2024.

## More efficient actor heap garbage collection

We have improved the actor heap garbage collection process
to make it more efficient by deferring some initialization
work and handling as part of the normal garbage collection
mark and sweep passes.

## Fix compiler crash related to explicit FFI return types

After we re-added the ability to override the return type for FFI function calls in a [previous release](https://github.com/ponylang/ponyc/releases/tag/0.50.0), we forgot to reintroduce some checks in the compiler that ensured that the specified return types would be known to the code generation pass. This caused some programs that introduced a new type (for example, a bare lambda) in the context of a FFI return type to crash the compiler. This is now fixed.

## Support void* (Pointer[None]) parameters in bare lambdas and functions

Unlike conventional FFI functions, bare lambdas `@{(...)}` and bare functions `fun @bare() => ...` did not support `Pointer[None]` parameters, Pony's equivalent of `void*`. This is despite the fact that like FFI functions, bare lambdas and functions are strictly intended for use with FFI calls and callbacks.

This commit allows bare lambdas and bare functions with `Pointer[None]` parameters to accept arguments of any `Pointer[A]` type for these parameters. Therefore, code like the following now works:

```pony
use @printf[I32](fmt: Pointer[None] tag, ...)

actor Main
  new create(env: Env) =>
    let printer = @{(fmt: Pointer[None] tag): I32 => @printf(fmt)}
    let cb = this~print()
    printer("Hello".cstring())
    cb(" world!\n".cstring())

  fun @print(fmt: Pointer[None]): I32 =>
    @printf(fmt)
```

## Systematic testing for the runtime

Pony is a concurrent and parallel language. Different actors can be run
at the same time on multiple CPUs. The Pony runtime coordinates all of
this interleaving of actors and contains a fair amount of complexity.
Runtime functionality such as the message queues and the backpressure
system rely on atomic operations which can be tricky to get right across
multiple platforms.

Systematic testing will allow us to coordinate the interleaving of the
multiple runtime scheduler threads in a deterministic and reproducible
manner. This ability to reproduce a particular runtime behavior will be
invaluable for debugging runtime issues.

The overall idea and some details of the implementation for systematic
testing has been shamelessly stolen from the Verona runtime (see:
https://github.com/microsoft/verona/blob/master/docs/explore.md#systematic-testing
for details). This implementation doesn't include replayable runtime
unit tests like Verona, but it sets a foundation for allowing replayable
runs of programs (and probably tests) for debugging runtime issues such
as backpressure/etc. Additionally, while all development and testing was
done on Linux, in theory this systematic testing functionality should
work on other operating systems (Windows, MacOS, Freebsd, etc) barring
issues related to lack of atomics for tracking the active thread and
whether a thread has stopped executing or not (unlikely to be an issue
on MacOS/Freebsd/other `pthread` based threading implementations).

### Instructions for using systematic testing...

The following instructions were tested on Linux but should in theory
function correctly on other *nix environments that use the Pony
`make`/`cmake` based build system.

Start with a clean environment:
```
make clean config=debug
```

Configure with `systematic testing` and `pthreads` enabled:
```
make configure config=debug use=scheduler_scaling_pthreads,systematic_testing
```

Build:
```
make build config=debug
```

Compile `helloworld`:
```
./build/debug-systematic_testing/ponyc examples/helloworld/
```

Run `helloworld`:
```
./helloworld
```

Output will look something like:

```
me@home:~/ponyc$ ./helloworld
Systematic testing using seed: 360200870782547...
(rerun with `<app> --ponysystematictestingseed 360200870782547` to reproduce)
<SNIPPED LOTS OF OUTPUT>
thread 139871784978176: yielding to thread: 139871776585472.. next_index: 3
Hello, world.
thread 139871776585472: yielding to thread: 139871768192768.. next_index: 4
<SNIPPED LOTS OF OUTPUT>
Systematic testing successfully finished!
me@home:~/ponyc$ 
```

