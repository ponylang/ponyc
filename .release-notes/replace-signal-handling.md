## Replace the signal handling system

The `signals` package's signal handling system has been replaced, implementing [RFC #87](https://github.com/ponylang/rfcs/blob/main/text/0087-redesign-signal-handling.md).

What the new system provides:

- Signal handling is capability-secure: creating a `SignalHandler`, raising a signal, and disposing a handler all require a `SignalAuth` capability, derived from `AmbientAuth`.
- Signal numbers are validated before they can be registered: `SignalHandler` takes a `HandleableSignal`, and `MakeHandleableSignal` rejects fatal signals like `SIGSEGV`, uncatchable ones like `SIGKILL`, and unknown numbers. `SIGUSR2` is accepted only on `scheduler_scaling_pthreads` builds, where the runtime leaves it free; on other builds the runtime reserves it for its scheduler and `Sig.usr2()` is a compile error. Previously, a `SIGUSR2` handler could be registered on a build where the runtime had reserved it and would silently never fire.
- Multiple actors can subscribe to the same signal: up to 16 subscribers per signal number, all notified when the signal fires.
- Registration failure is reported instead of silent: if the 16-subscriber limit is reached or the operating system refuses the registration, the notify's new `registration_failed` method is called with the reason — `SignalSubscriberLimit` is transient (a slot opens when another subscriber unsubscribes), `SignalRegistrationRefused` is permanent — and the handler is automatically disposed without `apply` ever running.
- The notify's end-of-life callback is now named `disposed` (previously `dispose`) and fires once the runtime has finished unregistering the handler, not when disposal was requested. If the handler was the signal's last subscriber, the disposition the signal had before it was first handled is already restored when `disposed` runs — the operating system default for most signals, and `SIG_IGN` for one the runtime keeps ignored like `SIGPIPE` — so a program can clean up on `SIGTERM`, dispose the handler, and re-raise the signal from the callback to end the process the way an unhandled `SIGTERM` ends it. A handler disposed while the runtime itself is shutting down may never receive the callback, since the process is already exiting.
- A signal arriving during runtime shutdown no longer races the runtime's teardown into freed memory: the runtime drops a signal that arrives while teardown is committing and restores each still-registered handler's signal to its earlier disposition on its way out.

Existing code needs updating. `SignalHandler` takes the auth and a validated signal, `raise` and `dispose` take the auth, and so do `SignalRaise` and the `ANSITerm` constructor.

Before:

```pony
use "signals"

let handler = SignalHandler(MyNotify, Sig.int())
handler.raise()
handler.dispose()
```

```pony
use "term"

let term = ANSITerm(notify, env.input)
```

After:

```pony
use "signals"

let auth = SignalAuth(env.root)
match MakeHandleableSignal(Sig.int())
| let sig: HandleableSignal =>
  let handler = SignalHandler(auth, MyNotify, sig)
  handler.raise(auth)
  handler.dispose(auth)
end
```

```pony
use "signals"
use "term"

let auth = SignalAuth(env.root)
let term = ANSITerm(auth, notify, env.input)
```

Two changes are easy to miss when migrating:

Because `dispose` on the handler now takes a parameter, `SignalHandler` no longer satisfies interfaces that expect a parameterless `dispose`, such as `DisposableActor` — for example, a `bureaucracy.Custodian` can no longer dispose a `SignalHandler` directly. Dispose the handler yourself where you hold the `SignalAuth`, or wrap it in a small actor that captures the auth and exposes a parameterless `dispose`.

A notify class that overrides `dispose` must rename it to `disposed`. The old method is no longer part of the `SignalNotify` interface, so an un-renamed override still compiles and is silently never called. `registration_failed` and `disposed` both have default implementations, so classes declaring `is SignalNotify` need no other changes; a class conforming to the interface only structurally must add the methods or declare `is SignalNotify`.
