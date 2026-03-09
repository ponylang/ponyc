use "constrained_types"
use "pony_test"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestValidSignalAcceptsHandleable)
    test(_TestValidSignalRejectsFatal)
    test(_TestValidSignalRejectsUncatchable)
    test(_TestValidSignalRejectsUnknown)
    test(_TestSignalINT)
    test(_TestMultipleHandlers)
    test(_TestDispose)
    test(_TestNotifyReturnsFalse)

class \nodoc\ iso _TestValidSignalAcceptsHandleable is UnitTest
  """
  Verify that all handleable signals pass SignalValidator.
  """
  fun name(): String => "signals/ValidSignal accepts handleable"

  fun apply(h: TestHelper) =>
    ifdef linux then
      _assert_valid(h, Sig.hup())
      _assert_valid(h, Sig.int())
      _assert_valid(h, Sig.quit())
      _assert_valid(h, Sig.pipe())
      _assert_valid(h, Sig.alrm())
      _assert_valid(h, Sig.term())
      _assert_valid(h, Sig.urg())
      _assert_valid(h, Sig.stkflt())
      _assert_valid(h, Sig.tstp())
      _assert_valid(h, Sig.cont())
      _assert_valid(h, Sig.chld())
      _assert_valid(h, Sig.ttin())
      _assert_valid(h, Sig.ttou())
      _assert_valid(h, Sig.io())
      _assert_valid(h, Sig.xcpu())
      _assert_valid(h, Sig.xfsz())
      _assert_valid(h, Sig.vtalrm())
      _assert_valid(h, Sig.prof())
      _assert_valid(h, Sig.winch())
      _assert_valid(h, Sig.pwr())
      _assert_valid(h, Sig.usr1())
      _assert_valid(h, Sig.sys())
    elseif bsd or osx then
      _assert_valid(h, Sig.hup())
      _assert_valid(h, Sig.int())
      _assert_valid(h, Sig.quit())
      _assert_valid(h, Sig.emt())
      _assert_valid(h, Sig.pipe())
      _assert_valid(h, Sig.alrm())
      _assert_valid(h, Sig.term())
      _assert_valid(h, Sig.urg())
      _assert_valid(h, Sig.tstp())
      _assert_valid(h, Sig.cont())
      _assert_valid(h, Sig.chld())
      _assert_valid(h, Sig.ttin())
      _assert_valid(h, Sig.ttou())
      _assert_valid(h, Sig.io())
      _assert_valid(h, Sig.xcpu())
      _assert_valid(h, Sig.xfsz())
      _assert_valid(h, Sig.vtalrm())
      _assert_valid(h, Sig.prof())
      _assert_valid(h, Sig.winch())
      _assert_valid(h, Sig.info())
      _assert_valid(h, Sig.usr1())
      _assert_valid(h, Sig.sys())
    end

  fun _assert_valid(h: TestHelper, sig: U32) =>
    match MakeValidSignal(sig)
    | let _: ValidSignal => None
    | let _: ValidationFailure =>
      h.fail("signal " + sig.string() + " should be valid")
    end

class \nodoc\ iso _TestValidSignalRejectsFatal is UnitTest
  """
  Verify that fatal signals are rejected by SignalValidator.
  """
  fun name(): String => "signals/ValidSignal rejects fatal"

  fun apply(h: TestHelper) =>
    ifdef linux or bsd or osx then
      _assert_invalid(h, Sig.ill())
      _assert_invalid(h, Sig.trap())
      _assert_invalid(h, Sig.abrt())
      _assert_invalid(h, Sig.fpe())
      _assert_invalid(h, Sig.bus())
      _assert_invalid(h, Sig.segv())
    end

  fun _assert_invalid(h: TestHelper, sig: U32) =>
    match MakeValidSignal(sig)
    | let _: ValidSignal =>
      h.fail("signal " + sig.string() + " should be rejected (fatal)")
    | let _: ValidationFailure => None
    end

class \nodoc\ iso _TestValidSignalRejectsUncatchable is UnitTest
  """
  Verify that uncatchable signals are rejected by SignalValidator.
  """
  fun name(): String => "signals/ValidSignal rejects uncatchable"

  fun apply(h: TestHelper) =>
    ifdef linux or bsd or osx then
      _assert_invalid(h, Sig.kill())
      _assert_invalid(h, Sig.stop())
    end

  fun _assert_invalid(h: TestHelper, sig: U32) =>
    match MakeValidSignal(sig)
    | let _: ValidSignal =>
      h.fail("signal " + sig.string() + " should be rejected (uncatchable)")
    | let _: ValidationFailure => None
    end

class \nodoc\ iso _TestValidSignalRejectsUnknown is UnitTest
  """
  Verify that arbitrary unknown signal numbers are rejected.
  """
  fun name(): String => "signals/ValidSignal rejects unknown"

  fun apply(h: TestHelper) =>
    _assert_invalid(h, 0)
    _assert_invalid(h, 200)
    _assert_invalid(h, U32.max_value())

  fun _assert_invalid(h: TestHelper, sig: U32) =>
    match MakeValidSignal(sig)
    | let _: ValidSignal =>
      h.fail("signal " + sig.string() + " should be rejected (unknown)")
    | let _: ValidationFailure => None
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
    let auth = SignalAuth(h.env.root)
    match MakeValidSignal(Sig.int())
    | let sig: ValidSignal =>
      let signal = SignalHandler(auth, _TestSignalNotify(h), sig)
      signal.raise(auth)
      _signal = signal
      h.long_test(10_000_000_000)
    | let _: ValidationFailure =>
      h.fail("SIGINT should be a valid signal")
    end

  fun timed_out(h: TestHelper) =>
    let auth = SignalAuth(h.env.root)
    try
      (_signal as SignalHandler).dispose(auth)
    end
    h.fail("timeout")
    h.complete(false)

class \nodoc\ _TestMultiHandlerNotify is SignalNotify
  let _h: TestHelper
  let _action: String

  new iso create(h: TestHelper, action: String) =>
    _h = h
    _action = action

  fun ref apply(count: U32): Bool =>
    _h.complete_action(_action)
    true

class \nodoc\ iso _TestMultipleHandlers is UnitTest
  """
  Verify that multiple handlers for the same signal all get notified.
  Handlers return true to stay registered; the test disposes them on
  completion to avoid auto-dispose interfering with the second handler.
  """
  var _signal1: (SignalHandler | None) = None
  var _signal2: (SignalHandler | None) = None

  fun name(): String => "signals/multiple handlers"

  fun ref apply(h: TestHelper) =>
    let auth = SignalAuth(h.env.root)
    h.expect_action("handler1")
    h.expect_action("handler2")
    match MakeValidSignal(Sig.int())
    | let sig: ValidSignal =>
      let s1 = SignalHandler(auth,
        _TestMultiHandlerNotify(h, "handler1"), sig)
      let s2 = SignalHandler(auth,
        _TestMultiHandlerNotify(h, "handler2"), sig)
      // Both handlers must raise independently. Actor constructors run
      // asynchronously, so s2's create might not have executed when s1's
      // raise fires the signal. Having each handler raise guarantees that
      // handler's own constructor has run (same-actor message ordering)
      // before its raise executes.
      s1.raise(auth)
      s2.raise(auth)
      _signal1 = s1
      _signal2 = s2
      h.long_test(10_000_000_000)
    | let _: ValidationFailure =>
      h.fail("SIGINT should be a valid signal")
    end

  fun timed_out(h: TestHelper) =>
    let auth = SignalAuth(h.env.root)
    try (_signal1 as SignalHandler).dispose(auth) end
    try (_signal2 as SignalHandler).dispose(auth) end
    h.fail("timeout")
    h.complete(false)

class \nodoc\ _TestDisposeNotify is SignalNotify
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref apply(count: U32): Bool =>
    true

  fun ref dispose() =>
    // Dispose callback was invoked — test passes
    _h.complete(true)

class \nodoc\ iso _TestDispose is UnitTest
  """
  Verify that disposing a SignalHandler calls the notify's dispose method.
  """

  fun name(): String => "signals/dispose"

  fun ref apply(h: TestHelper) =>
    let auth = SignalAuth(h.env.root)
    match MakeValidSignal(Sig.int())
    | let sig: ValidSignal =>
      let signal = SignalHandler(auth, _TestDisposeNotify(h), sig)
      signal.dispose(auth)
      h.long_test(10_000_000_000)
    | let _: ValidationFailure =>
      h.fail("SIGINT should be a valid signal")
    end

  fun timed_out(h: TestHelper) =>
    h.fail("dispose callback was not called")
    h.complete(false)

class \nodoc\ _TestReturnsFalseNotify is SignalNotify
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref apply(count: U32): Bool =>
    false

  fun ref dispose() =>
    _h.complete(true)

class \nodoc\ iso _TestNotifyReturnsFalse is UnitTest
  """
  Verify that a notify returning false auto-disposes the handler.
  """
  var _signal: (SignalHandler | None) = None

  fun name(): String => "signals/notify returns false"

  fun ref apply(h: TestHelper) =>
    let auth = SignalAuth(h.env.root)
    match MakeValidSignal(Sig.int())
    | let sig: ValidSignal =>
      let signal = SignalHandler(auth, _TestReturnsFalseNotify(h), sig)
      signal.raise(auth)
      _signal = signal
      h.long_test(10_000_000_000)
    | let _: ValidationFailure =>
      h.fail("SIGINT should be a valid signal")
    end

  fun timed_out(h: TestHelper) =>
    let auth = SignalAuth(h.env.root)
    try
      (_signal as SignalHandler).dispose(auth)
    end
    h.fail("timeout")
    h.complete(false)
