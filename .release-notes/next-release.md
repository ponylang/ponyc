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

