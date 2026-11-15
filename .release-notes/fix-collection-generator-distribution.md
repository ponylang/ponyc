## Fix collection generator length distribution regression

Collection generators (`array_of`, `seq_of`, `set_of`, `map_of`, `byte_string`, `utf32_codepoint_string`, and their variants) produced sizes averaging 1 above `from`, regardless of `to`. `array_of(u8(), 0, 100)` produced an empty array half the time and almost never produced one longer than 14 elements. The `to` bound had no practical effect on the distribution.

Sizes are now uniformly distributed on `[from, to]`, matching the behavior through 0.70.x.

`Randomness.biased_bool(numerator, denominator)` is now available for custom generators that use the boolean-per-element pattern and need to control their size distribution.

```pony
let cont = rnd.biased_bool(remaining, remaining + 1)
```
