"""
# Signals package

The Signals package provides support for handling Unix style signals.
For each signal that you want to handle, you need to create a `SignalHandler`
and a corresponding `SignalNotify` object. Each SignalHandler runs as it own
actor and upon receiving the signal will call its corresponding
`SignalNotify`'s apply method.

## Example program

The following program will listen for the TERM signal and output a message to
standard out if it is received.

```pony
use "signals"

actor Main
  new create(env: Env) =>
    // Create a TERM handler
    let signal = SignalHandler(TermHandler(env), Sig.term())
    // Raise TERM signal
    signal.raise()

class TermHandler is SignalNotify
  let _env: Env

  new iso create(env: Env) =>
    _env = env

  fun ref apply(count: U32): Bool =>
    _env.out.print("TERM signal received")
    true
```

## Signal portability

The `Sig` primitive provides support for portable signal handling across Linux,
FreeBSD and OSX. Signals are not supported on Windows and attempting to use
them will cause a compilation error.

## Shutting down handlers

Unlike a `TCPConnection` and other forms of input receiving, creating a
`SignalHandler` will not keep your program running. As such, you are not
required to call `dispose` on your signal handlers in order to shutdown your
program.

"""
