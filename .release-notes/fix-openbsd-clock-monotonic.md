## Fix incorrect CLOCK_MONOTONIC value on OpenBSD

On OpenBSD, `CLOCK_MONOTONIC` is defined as `3`, not `4` like on FreeBSD and DragonFly BSD. Pony's `time` package was using `4` for all BSDs, which meant that on OpenBSD, every call to `Time.nanos()`, `Time.millis()`, and `Time.micros()` was actually reading `CLOCK_THREAD_CPUTIME_ID` — the CPU time consumed by the calling thread — instead of monotonic wall-clock time.

The practical result is that the `Timers` system, which relies on `Time.nanos()` for scheduling, would misfire on OpenBSD. Timers set for wall-clock durations would instead fire based on how much CPU time the scheduler thread had burned. For a mostly-idle program, a 60-second timer could take far longer than 60 seconds to fire. Any other code that depends on monotonic time would see similarly wrong values. Hilarity ensues.
