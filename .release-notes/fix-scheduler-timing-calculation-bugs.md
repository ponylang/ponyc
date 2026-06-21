## Fix scheduler timing calculation bugs

The runtime scheduler makes several decisions based on how much time has elapsed, measured with a CPU cycle counter. Two of those calculations were wrong.

On 32-bit ARM, the counter is a hardware cycle counter that periodically wraps back to zero. Several scheduler checks subtracted two readings without accounting for the wrap, so for one iteration after each wrap the elapsed time came out as a nonsensical huge value — briefly suspending a scheduler thread early, sending an internal "no more work" notification early, or printing runtime statistics ahead of schedule before self-correcting. The runtime now accounts for the wrap, so the scheduler no longer misreads elapsed time as the counter wraps.

Separately, on every platform, the runtime statistics interval (available when the runtime is built with stats tracking and selected with the stats-interval option) was computed at the wrong scale and at too narrow an integer width: statistics printed roughly a thousand times more often than the requested interval, and a large interval could overflow to a small value. Statistics now print at the requested interval.

Programs that run on other platforms and don't enable runtime statistics were unaffected.
