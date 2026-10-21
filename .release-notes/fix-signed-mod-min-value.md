## Fix signed mod producing wrong results when divisor is min_value

`mod` and `%%` on signed integers returned wrong results when the divisor was `min_value()` and the dividend was positive. The result depended on the optimization level — debug builds produced the correct value, release builds did not. `modc` had the same problem.

```pony
I32(1).mod(I32.min_value()) // was: wrong in release builds
                             // now: -2147483647
```
