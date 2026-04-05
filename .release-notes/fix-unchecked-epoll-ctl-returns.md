## Fix rare silent connection hangs on Linux

On Linux, if the OS failed to register an I/O event (due to resource exhaustion or an FD race), the failure was silently ignored. Actors waiting for network events that were never registered would hang indefinitely with no error. This could appear as connections that never complete, listeners that stop accepting, or timers that stop firing — with no indication of what went wrong.

The runtime now detects these registration failures and notifies the affected actor, which tears down cleanly — the same as any other I/O failure. Stdlib consumers like `TCPConnection` and `TCPListener` handle this automatically.

Also fixes the ASIO backend init to correctly detect `epoll_create1` and `eventfd` failures (previously checked for 0 instead of -1), and to clean up all resources on partial init failure.
