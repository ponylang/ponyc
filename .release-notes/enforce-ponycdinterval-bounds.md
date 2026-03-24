## Enforce documented bounds for --ponycdinterval

The help text for `--ponycdinterval` has always said the minimum is 10 ms and the maximum is 1000 ms, but the runtime never actually enforced either bound on the command line. You could pass any non-negative value and it would be silently clamped deep in the cycle detector initialization. Values above ~2147 would also overflow during an internal conversion to CPU cycles, producing nonsensical detection intervals.

The documented bounds are now enforced. Passing a value outside [10, 1000] on the command line will produce an error. Values set via `RuntimeOptions` continue to be clamped to the valid range.
