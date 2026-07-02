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
    // Returning false unsubscribes and disposes this handler.
    false

  fun ref dispose() =>
    _env.out.print(_name + " disposed")

actor Main
  """
  Demonstrates the signals package:
  - Creating a SignalAuth capability from AmbientAuth
  - Validating signal numbers with MakeValidSignal
  - Registering multiple handlers for the same signal
  - Raising a signal programmatically
  - Unsubscribing by returning false from a notify's apply

  The program registers two handlers for SIGINT and raises it once.
  handler-1 is guaranteed to receive the signal: the raise goes through
  the handler itself, and messages to the same actor are processed in
  order, so its registration completes before the raise runs. handler-2
  has no such ordering — actor constructors run asynchronously — so it
  usually receives the signal too, but can miss it if the raise outraces
  its registration. Handlers that receive the signal print, unsubscribe
  by returning false, and print again when disposed. handler-1 is created
  with `wait = true`, which keeps the program alive until that handler is
  disposed — without it, the program could exit before the signal is
  delivered at all.
  """
  new create(env: Env) =>
    let auth = SignalAuth(env.root)

    match MakeValidSignal(Sig.int())
    | let sig: ValidSignal =>
      let h1 =
        SignalHandler(auth, Handler(env, "handler-1"), sig where wait = true)
      let h2 = SignalHandler(auth, Handler(env, "handler-2"), sig)

      env.out.print("Two handlers registered for SIGINT.")
      env.out.print("Raising SIGINT programmatically...")
      h1.raise(auth)
    | let f: ValidationFailure =>
      env.out.print("Failed to validate SIGINT")
      for e in f.errors().values() do
        env.out.print("  " + e)
      end
    end
