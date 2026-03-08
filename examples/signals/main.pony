use "constrained_types"
use "signals"

class Handler is SignalNotify
  let _env: Env
  let _name: String

  new iso create(env: Env, name: String) =>
    _env = env
    _name = name

  fun ref apply(count: U32): Bool =>
    _env.out.print(_name + " received signal (count: " + count.string() + ")")
    true

  fun ref dispose() =>
    _env.out.print(_name + " disposed")

actor Main
  """
  Demonstrates the signals package:
  - Creating a SignalAuth capability from AmbientAuth
  - Validating signal numbers with MakeValidSignal
  - Registering multiple handlers for the same signal
  - Raising a signal programmatically
  - Disposing a handler

  Run the program and press Ctrl-C to send SIGINT, or wait for the
  programmatic raise to fire both handlers.
  """
  new create(env: Env) =>
    let auth = SignalAuth(env.root)

    match MakeValidSignal(Sig.int())
    | let sig: ValidSignal =>
      let h1 = SignalHandler(auth, Handler(env, "handler-1"), sig)
      let h2 = SignalHandler(auth, Handler(env, "handler-2"), sig)

      env.out.print("Two handlers registered for SIGINT.")
      env.out.print("Raising SIGINT programmatically...")
      h1.raise(auth)

      // Dispose handler-2 to show cleanup
      h2.dispose(auth)
    | let f: ValidationFailure =>
      env.out.print("Failed to validate SIGINT")
      for e in f.errors().values() do
        env.out.print("  " + e)
      end
    end
