## Enforce documented maximum for --ponysuspendthreshold

The help text for `--ponysuspendthreshold` has always said the maximum value is 1000 ms, but the runtime never actually enforced it. You could pass any value and it would be accepted. Values above ~4294 would silently overflow during an internal conversion to CPU cycles, producing nonsensical thresholds.

The documented maximum of 1000 ms is now enforced. Passing a value above 1000 on the command line will produce an error. Values set via `RuntimeOptions` are clamped to 1000.
