## Fix silent timer hangs on Linux

On Linux, if a timer system call failed (due to resource exhaustion or other system error), the failure was silently ignored. Actors waiting for timer notifications would hang indefinitely with no error — timers that should fire simply never did.

The runtime now detects timer setup and arming failures and notifies the affected actor, which tears down cleanly — the same as any other I/O failure. Stdlib consumers like `Timers` handle this automatically.
