"""
# Signals package

The Signals package provides support for handling Unix style signals with
capability security and support for multiple handlers per signal.

## Overview

For each signal that you want to handle, you need to create a `SignalHandler`
and a corresponding `SignalNotify` object. Multiple `SignalHandler` actors can
be registered for the same signal — all registered handlers will be notified
when the signal is received, in no particular order.

Signal handling requires a `SignalAuth` capability derived from `AmbientAuth`,
consistent with how other I/O primitives in the standard library handle
resource access.

Signal numbers must be validated through `MakeValidSignal` before they can
be used with `SignalHandler`. This prevents registration of handlers for
fatal signals (SIGILL, SIGTRAP, SIGABRT, SIGFPE, SIGBUS, SIGSEGV) and
uncatchable signals (SIGKILL, SIGSTOP) that cannot be meaningfully handled
via the ASIO mechanism.

## Example program

```pony
use "constrained_types"
use "signals"

actor Main
  new create(env: Env) =>
    let auth = SignalAuth(env.root)

    match MakeValidSignal(Sig.term())
    | let sig: ValidSignal =>
      // Multiple handlers for the same signal
      SignalHandler(auth, LogHandler(env.out), sig)
      SignalHandler(auth, CleanupHandler(env.out), sig where wait = true)
    | let err: ValidationFailure =>
      env.err.print("Cannot handle this signal")
    end

class LogHandler is SignalNotify
  let _out: OutStream

  new iso create(out: OutStream) =>
    _out = out

  fun ref apply(count: U32): Bool =>
    _out.print("Signal received, count: " + count.string())
    true

class CleanupHandler is SignalNotify
  let _out: OutStream

  new iso create(out: OutStream) =>
    _out = out

  fun ref apply(count: U32): Bool =>
    _out.print("Cleaning up...")
    false

  fun ref dispose() =>
    _out.print("Cleanup handler disposed")
```

## Signal portability

The `Sig` primitive provides support for portable signal handling across Linux,
FreeBSD and OSX. Signals are not supported on Windows and attempting to use
them will cause a compilation error.

## Disposing handlers

Disposing a `SignalHandler` unsubscribes it from the signal. This is important
because the signal dispatch mechanism holds a reference to each subscriber —
without explicit disposal, handlers will never be garbage collected. If a
`SignalHandler` is created with `wait = true`, disposing it (or returning
`false` from the notifier) is required to allow the program to terminate.
"""
