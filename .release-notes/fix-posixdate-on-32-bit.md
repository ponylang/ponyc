## Fix PosixDate on 32-bit

On systems with 32-bit `time_t` (such as 32-bit ARM Linux), every `PosixDate` produced wrong field values regardless of the input. `PosixDate(0, 0)` returned year 1905 instead of 1970, and `time()` returned `-2037645953` instead of `0`.

The upper bound for dates is now clamped to the `time_t` range on each platform. On 32-bit `time_t` systems, this means the maximum representable date is 2038-01-19 03:14:07 UTC (the Y2038 limit) rather than year 2147483647.
