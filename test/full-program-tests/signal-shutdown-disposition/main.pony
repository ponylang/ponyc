use "constrained_types"
use "signals"

use @signal[Pointer[None]](signum: I32, handler: Pointer[None])
use @pony_exitcode[None](code: I32)

// An undisposed wait=false SignalHandler doesn't block quiescence, so the
// runtime shuts down with the signal still registered. The dispatch-exit
// sweep (#5564) must restore the OS-default disposition before the asio
// backend is freed. Finalizers run after that shutdown completes, so _final
// can probe the disposition: signal() returns the previous handler, and
// SIG_DFL is the null pointer on every supported platform. Exit code 42 is
// set only when the probe sees SIG_DFL — if the sweep regresses, the
// runtime's handler (epoll/Windows) or SIG_IGN (kqueue) is still installed,
// the probe returns non-null, and the program exits 0. If SIGTERM
// registration itself broke, this test would pass vacuously; the signals
// package tests cover registration and delivery.

class _NoOpNotify is SignalNotify
  new iso create() =>
    None

  fun ref apply(count: U32): Bool =>
    true

actor Main
  new create(env: Env) =>
    let auth = SignalAuth(env.root)
    match MakeValidSignal(Sig.term())
    | let sig: ValidSignal =>
      SignalHandler(auth, _NoOpNotify, sig where wait = false)
    | let _: ValidationFailure =>
      None
    end

  fun _final() =>
    if @signal(Sig.term().i32(), USize(0)).is_null() then
      @pony_exitcode(42)
    end
