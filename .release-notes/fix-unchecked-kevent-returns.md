## Fix rare silent connection hangs on macOS and BSD

On macOS and BSD, if the OS failed to fully register an I/O event (due to resource exhaustion or an FD race), the failure was silently ignored. Actors waiting for network events that were never registered would hang indefinitely with no error. This could appear as connections that never complete, listeners that stop accepting, or timers that stop firing — with no indication of what went wrong.

The runtime now detects these registration failures and notifies the affected actor, which tears down cleanly — the same as any other I/O failure. Stdlib consumers like `TCPConnection` and `TCPListener` handle this automatically.

If you implement `AsioEventNotify` outside the stdlib, you can now detect registration failures with the new `AsioEvent.errored` predicate. Without handling it, a failure is silently ignored (the same behavior as before, but now you have the option to detect it):

```pony
be _event_notify(event: AsioEventID, flags: U32, arg: U32) =>
  if AsioEvent.errored(flags) then
    // Registration failed — tear down
    _close()
    return
  end
  // ... normal event handling
```
