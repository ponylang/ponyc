use "pony_test"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestSignalINT)
    // Sig.usr2() is a compile error except on scheduler_scaling_pthreads builds
    // that have SIGUSR2 (not Windows); register the test only where it runs.
    ifdef "scheduler_scaling_pthreads" and not windows then
      test(_TestSignalUSR2)
    end

class \nodoc\ _TestSignalNotify is SignalNotify
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref apply(count: U32): Bool =>
    _h.complete(true)
    false

class \nodoc\ iso _TestSignalINT is UnitTest
  var _signal: (SignalHandler | None) = None

  fun name(): String => "signals/INT"

  fun ref apply(h: TestHelper) =>
    let signal = SignalHandler(_TestSignalNotify(h), Sig.int())
    signal.raise()
    _signal = signal
    h.long_test(2_000_000_000) // 2 second timeout

  fun timed_out(h: TestHelper) =>
    try
      (_signal as SignalHandler).dispose()
    end

    h.fail("timeout")
    h.complete(false)

class \nodoc\ iso _TestSignalUSR2 is UnitTest
  var _signal: (SignalHandler | None) = None

  fun name(): String => "signals/USR2"

  fun ref apply(h: TestHelper) =>
    // Gated to match the registration in tests(): the runtime leaves SIGUSR2
    // free only on scheduler_scaling_pthreads builds, and it exists only off
    // Windows, so that is where Sig.usr2() yields a usable signal number.
    ifdef "scheduler_scaling_pthreads" and not windows then
      let signal = SignalHandler(_TestSignalNotify(h), Sig.usr2())
      signal.raise()
      _signal = signal
      h.long_test(2_000_000_000) // 2 second timeout
    end

  fun timed_out(h: TestHelper) =>
    try
      (_signal as SignalHandler).dispose()
    end

    h.fail("timeout")
    h.complete(false)
