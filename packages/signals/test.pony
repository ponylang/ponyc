use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestSignalINT)

class _TestSighupNotify is SignalNotify
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref apply(count: U32): Bool =>
    _h.complete(true)
    false

class iso _TestSignalINT is UnitTest
  var _signal: (SignalHandler | None) = None

  fun name(): String => "signals/INT"

  fun ref apply(h: TestHelper): TestResult =>
    let signal = SignalHandler(_TestSighupNotify(h), 2)
    signal.raise()
    _signal = signal
    2_000_000_000 // 2 second timeout

  fun timedout(h: TestHelper) =>
    try
      (_signal as SignalHandler).dispose()
      h.assert_failed("timeout")
      h.complete(false)
    end
