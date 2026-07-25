"""
# Signals package

The Signals package provides support for handling Unix style signals with
capability security and support for multiple handlers per signal.

## Overview

For each signal that you want to handle, you need to create a `SignalHandler`
and a corresponding `SignalNotify` object. Multiple `SignalHandler` actors can
be registered for the same signal — up to 16 per signal number — and all
registered handlers will be notified when the signal is received, in no
particular order. A handler that cannot be registered (the limit is reached,
or the runtime fails to register with the operating system) is notified via
its notify's `registration_failed`, with the reason, and automatically
disposed; `apply` will not have run.

Signal handling requires a `SignalAuth` capability derived from `AmbientAuth`,
consistent with how other I/O primitives in the standard library handle
resource access.

Signal numbers must be validated through `MakeHandleableSignal` before they can
be used with `SignalHandler`. Validation is a per-platform whitelist of
signals that can be meaningfully handled via the ASIO mechanism: it rejects
fatal signals (SIGILL, SIGTRAP, SIGABRT, SIGFPE, SIGBUS, SIGSEGV),
uncatchable signals (SIGKILL, SIGSTOP), SIGUSR2 (reserved by the Pony
runtime for scheduler sleep/wake, so a handler could never fire), and any
number outside the whitelist.

## Example program

```pony
use "constrained_types"
use "signals"

actor Main
  new create(env: Env) =>
    let auth = SignalAuth(env.root)

    match MakeHandleableSignal(Sig.term())
    | let sig: HandleableSignal =>
      // Multiple handlers for the same signal
      SignalHandler(auth, LogHandler(env.out), sig)
      SignalHandler(auth, CleanupHandler(env.out), sig where wait = true)
    | let _: ValidationFailure =>
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

  fun ref disposed() =>
    _out.print("Cleanup handler disposed")
```

## Signal portability

The `Sig` primitive provides support for portable signal handling across Linux,
FreeBSD, Haiku and OSX. On Windows only the subset of signals the C runtime supports
is available: the accessors that do not depend on the platform (such as `Sig.int()`
for `SIGINT`) work, while platform-specific accessors (for example
`Sig.trap()`) raise a compile error there. A few platform-independent
accessors (such as `Sig.hup()`) compile on Windows but name signals it does
not have. Validation resolves this: on Windows only `Sig.int()` and
`Sig.term()` are accepted by `MakeHandleableSignal` — everything else, including
signals the C runtime knows about but treats as fatal (such as `SIGABRT`),
is rejected.

## Disposing handlers

Disposing a `SignalHandler` unsubscribes it from the signal. This is important
because the signal dispatch mechanism holds a reference to each subscriber —
without explicit disposal, handlers will never be garbage collected. If a
`SignalHandler` is created with `wait = true`, disposing it (or returning
`false` from the notifier) is required to allow the program to terminate.
Note that raising a signal after every handler for it has been disposed
delivers it to the disposition the signal had before it was first handled —
the operating system default for most signals (process death for the
terminating ones), and SIG_IGN for one the runtime keeps ignored, like
SIGPIPE.
"""
