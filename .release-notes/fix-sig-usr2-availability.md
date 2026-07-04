## Fix `Sig.usr2()` reporting SIGUSR2 availability backwards

`Sig.usr2()` in the `signals` package had its availability reversed with respect to how the Pony runtime uses SIGUSR2, so it was unusable in every configuration.

On the default Linux and BSD builds, the runtime reserves SIGUSR2 for its own scheduler use, so a handler registered for it never fires — yet `Sig.usr2()` compiled and returned a signal number, silently handing you a handler that could never run. On macOS (and other `scheduler_scaling_pthreads` builds), the runtime leaves SIGUSR2 free, but `Sig.usr2()` was a compile error claiming the signal was reserved.

`Sig.usr2()` now returns a signal number on `scheduler_scaling_pthreads` builds, including macOS where that mode is always on, and is a compile error on the default builds where the runtime reserves the signal — so a misuse shows up when you compile rather than as a handler that silently never fires. One configuration is still not covered: `runtime_tracing` builds use SIGUSR2 themselves regardless of scheduler mode, and `Sig.usr2()` has no way to detect that, so it should not be used on a `runtime_tracing` build.
