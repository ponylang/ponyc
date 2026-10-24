## Fix Format.float ignoring PrefixSign and PrefixSpace

`Format.float` accepted a `PrefixNumber` parameter but ignored it. `PrefixSign` and `PrefixSpace` had no effect on the output, even though `Format.int` honored both correctly.

```pony
Format.float[F64](1.0 where prefix = PrefixSign)   // was: "1"  now: "+1"
Format.float[F64](1.0 where prefix = PrefixSpace)  // was: "1"  now: " 1"
Format.float[F64](-1.0 where prefix = PrefixSign)  // "-1" (unchanged)
```
