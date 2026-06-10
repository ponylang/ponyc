## Reject use=dtrace at configure time on DragonFly and OpenBSD

DTrace isn't supported on DragonFly BSD or OpenBSD. `make configure use=dtrace` now rejects the option on those platforms with a clear, platform-specific message rather than a confusing build failure or a generic error.
