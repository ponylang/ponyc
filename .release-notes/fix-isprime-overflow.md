## Fix IsPrime returning wrong results for primes near the type maximum

`IsPrime` reported primes near the maximum of an unsigned integer type as composite. For `U64`, primes near `U64.max_value()` caused the function to run indefinitely instead of returning.

```pony
IsPrime[U32](4294967291) // was false, now true (4294967291 is the largest U32 prime)
IsPrime[U16](65521)      // was false, now true (65521 is the largest U16 prime)
```
