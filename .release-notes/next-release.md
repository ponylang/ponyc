## Fix Windows programs never finishing when stdin has ended

On Windows, a program that subscribed to stdin after stdin had ended could hang forever at 0% CPU. The new subscription never received a read, so the end of the input never reached the program, and the program never exited. A program run with its input redirected from `NUL` had a matching problem: the end of its input never arrived either, and it spun at 100% CPU instead of exiting.

A Windows program reading redirected stdin now reaches the end of its input the way it does on Linux and macOS.

## Fix Windows programs stalling while reading stdin from a pipe

On Windows, a read of stdin from a pipe did not return until the program on the other end of the pipe wrote a byte or closed the pipe. With one scheduler thread, nothing else in the program ran in the meantime: no timer fired and no other actor ran. With the default number of scheduler threads, the rest of the program kept running, but `env.input.dispose()` had no effect until the other end wrote a byte or closed the pipe, so the program kept reading stdin and could not exit.

The rest of a Windows program now runs while it waits for input on stdin, and `env.input.dispose()` takes effect when you call it, as it already did on Linux and macOS. Console input, stdin redirected from a file, and stdin from NUL were never affected.

## Fix a new Windows stdin notifier getting a read posted for the one it replaced

On Windows, a program that disposed its stdin notifier and installed a new one could have the new notifier sent a read that was posted for the notifier it replaced.

Reading stdin through `env.input` and the standard library's `Stdin` actor, nothing was lost, duplicated, or reordered, so most programs saw no difference. A program that subscribed to stdin readiness through the asio event API directly could be sent a read posted for a notifier it had already replaced. A new notifier is now sent only the reads posted for it.

## Fix `F32.ldexp` linking on Windows

Any program that called `F32.ldexp` failed to build on Windows:

```console
unable to link: lld-link: error: undefined symbol: ldexpf
```

This has been fixed.

## Fix pony-lsp not working when your editor escapes characters in a file path

When your editor tells a language server which file you are editing, it escapes any character that is not legal in a URI. A space is sent as `%20`, `é` as `%C3%A9`. VS Code and emacs lsp-mode also escape the colon after a Windows drive letter, sending `C:` as `C%3A`.

pony-lsp did not undo that escaping. It used the escaped text as the file name, so it looked for a file whose name really did contain `%20`, found nothing, and returned an error for every request about that file. If your project path contained a space or a non-ASCII character, pony-lsp did not work on it. That was true on every platform. On Windows with VS Code, pony-lsp did not work on any project at all, because of the escaped colon after the drive letter.

pony-lsp also did not escape the paths it sent back, so a path containing a space produced a location that was not a valid URI.

pony-lsp now undoes the escaping in the URIs your editor sends, and escapes the paths it sends back. There is nothing you need to change.

## Fix pony-lsp not working when your editor sends your project's location as a URI

When your editor starts a language server, it tells the server where your project is. Some editors send a list of folders. Others send a single location instead, either as a plain path like `/home/you/myproject` or as a URI like `file:///home/you/myproject`.

pony-lsp read that single location as a plain path in both cases. When an editor sent a URI, pony-lsp searched for a directory whose name really did start with `file://`, found nothing, and registered no project at all. Every request about your code returned an error, so hover, go to definition, find references, and rename all did nothing.

vim-lsp sends a URI in its default configuration, so pony-lsp did not work there on a stock setup. Kate sends one when you have configured a project root. If your editor sends a folder list with your project in it, as VS Code does, pony-lsp already worked.

pony-lsp now accepts a project location in either form. There is nothing you need to change.

One case is not covered by this fix: an editor that sends an empty folder list alongside a project location still finds no project. Kate does this when you have not configured a project root.

## Correct the signature of `FloatingPoint.ldexp`

`FloatingPoint.ldexp` scales a floating-point value by a power of two (the inverse of `frexp`), but it took the value to scale as an explicit first argument and never used the receiver it was called on. Operations on a float elsewhere in `FloatingPoint` work on the receiver.

The value to scale has moved from the first argument to the receiver. `ldexp` now scales the value it is called on.

Before:

```pony
// the receiver (here F64(0)) was never used; `mantissa` was the value scaled
let scaled = F64(0).ldexp(mantissa, exponent)
```

After:

```pony
let scaled = mantissa.ldexp(exponent)
```

## Speed up type checking of large array literals

Type checking an array literal was quadratic in the number of elements, which made the type-check pass very slow for large literals -- a few thousand elements took many seconds. It is now linear in the number of elements: a 2000-element literal that spent about 93 seconds in the type checker now spends under half a second. Runtime behavior is unchanged.

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

## 32-bit ARM Linux is now a best-effort target

32-bit ARM Linux is no longer tested in CI. It is now a best-effort target: we build and test it periodically on real hardware rather than continuously in CI. Pony still builds and runs on 32-bit ARM and we don't intend to break it, but a change can break the 32-bit build between those checks without CI catching it.

## Fix spurious 'can't reuse name' errors when a multi-file package has a syntax error

When a package contained multiple `.pony` files and one of them had a syntax error, the compiler would produce spurious `can't reuse name` errors for unrelated valid code in the other files. This was most visible when a valid file reused a loop variable name across consecutive `for` loops — a perfectly legal pattern that the compiler would incorrectly flag.

The spurious errors no longer appear. You'll still see the real syntax error, but the compiler won't pile on with bogus errors from other files in the same package.

