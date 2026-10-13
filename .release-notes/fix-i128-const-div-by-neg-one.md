## Fix I128 constant division by -1 producing wrong result

`let r: I128 = 0 / -1` produced `I128.min_value()` instead of `0`. The compiler's constant-folding path compared 128-bit values through 64-bit intermediaries, and `I128.min_value()` has all-zero lower 64 bits — making `I128(0)` and `I128.min_value()` look equal. Any I128 constant that is a multiple of 2^64 divided by -1 was affected.
