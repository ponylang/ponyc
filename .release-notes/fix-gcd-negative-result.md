## Fix GreatestCommonDivisor returning negative values for signed inputs

`GreatestCommonDivisor` returned a negative value when the second argument was negative. The greatest common divisor is positive by definition.

```pony
GreatestCommonDivisor[I64](4, -6)?  // was: -2, now: 2
GreatestCommonDivisor[I64](-4, -6)? // was: -2, now: 2
GreatestCommonDivisor[I8](1, -1)?   // was: -1, now: 1
```

When both arguments are `min_value()` of a signed type, the positive result cannot be represented and the call now errors. Previously it silently returned the negative `min_value()`.
