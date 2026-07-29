use "constrained_types"
use "signals"

// A wait=true SignalHandler must keep the program alive until it is
// disposed, and returning false from the notify must dispose it. Exit code
// 42 is set only inside the notify, so passing requires the signal to have
// been DELIVERED before exit — if wait=true degrades to a no-op the program
// can exit 0 without delivery, and if the dispose chain regresses the
// program hangs; both directions go red.
//
// This raises SIGINT. CI runs the full-program tests under a debugger, so a
// signal raised here must also be in the pass-lists in
// .ci-scripts/test-debugger.sh, or the debugger stops on delivery and kills
// the leg.

class _ExitNotify is SignalNotify
  let _env: Env

  new iso create(env: Env) =>
    _env = env

  fun ref apply(count: U32): Bool =>
    _env.exitcode(42)
    false

actor Main
  new create(env: Env) =>
    let auth = SignalAuth(env.root)
    match MakeHandleableSignal(Sig.int())
    | let sig: HandleableSignal =>
      let handler =
        SignalHandler(auth, _ExitNotify(env), sig where wait = true)
      handler.raise(auth)
    | let _: ValidationFailure =>
      env.exitcode(1)
    end
