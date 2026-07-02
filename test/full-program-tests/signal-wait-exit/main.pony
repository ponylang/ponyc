use "constrained_types"
use "signals"

// A wait=true SignalHandler must keep the program alive until it is
// disposed, and returning false from the notify must dispose it. The
// assertion is the program exiting at all: a regression anywhere in the
// dispose -> cancel -> disposable -> destroy chain hangs this program and
// the test harness times out.

class _ExitNotify is SignalNotify
  new iso create() =>
    None

  fun ref apply(count: U32): Bool =>
    false

actor Main
  new create(env: Env) =>
    let auth = SignalAuth(env.root)
    match MakeValidSignal(Sig.int())
    | let sig: ValidSignal =>
      let handler = SignalHandler(auth, _ExitNotify, sig where wait = true)
      handler.raise(auth)
    | let _: ValidationFailure =>
      None
    end
