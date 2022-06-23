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

