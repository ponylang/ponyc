## Reject runtime-reserved signals in HandleableSignalValidator

`HandleableSignalValidator` now rejects signals the runtime reserves for its tracing thread-pause mechanism. On BSD and macOS, `Sig.info()` (SIGINFO) is rejected. On Linux, real-time signals 32 through 35 are rejected — the C library and the runtime reserve them.

Programs that registered a `SignalHandler` for one of these signals will now get a `ValidationFailure` from `MakeHandleableSignal` instead of silently replacing the runtime's handler.
