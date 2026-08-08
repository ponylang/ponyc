use "constrained_types"
use "pony_test"

use @signal[Pointer[None]](signum: I32, handler: Pointer[None])

// Signal handling is process-global: a signal's OS disposition and the
// runtime's table of subscribers for it are shared by every test here, not
// isolated per test. A test that leaves a handler registered when it finishes
// hands the next test a signal that is not in its default state.
//
// This matters for the tests that read a signal's disposition back: the
// runtime restores a signal's disposition only once its last subscriber
// unregisters, so those probes are correct only when no other handler for the
// same signal is still alive. So every test that registers a handler disposes
// it and waits for the unregistration to finish before it completes — the
// subscriber-limit test waits for every SIGTERM handler it created. That
// keeps the tests independent of the order they run in, including --shuffle.

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // Every signal a test here raises must also be in the debugger pass-lists
    // in .ci-scripts/test-debugger.sh. CI runs this suite under a debugger; a
    // raised signal missing from those lists stops the debugger on the first
    // delivery and kills the whole leg, while the tests keep passing
    // everywhere else. This suite raises SIGINT, SIGTERM, and SIGUSR2.
    test(_TestHandleableSignalAccepts)
    test(_TestHandleableSignalRejectsFatal)
    test(_TestHandleableSignalRejectsUncatchable)
    test(_TestHandleableSignalRejectsUnknown)
    test(_TestHandleableSignalRejectsReserved)
    test(_TestHandleableSignalRTBoundaries)
    test(_TestSignalINT)
    // SIGUSR2 is a compile error except on scheduler_scaling_pthreads builds
    // off Windows, where the runtime frees it; register the test only there.
    ifdef "scheduler_scaling_pthreads" and not windows then
      test(_TestSignalUSR2)
    end
    test(_TestOSRefusedRegistration)
    test(_TestMultipleHandlers)
    test(_TestDispose)
    test(_TestDisposeRestoresDefaultDisposition)
    // SIGPIPE is not handleable on Windows; the runtime ignores it off Windows.
    ifdef not windows then
      test(_TestDisposeRestoresIgnoredDisposition)
    end
    test(_TestNotifyReturnsFalse)
    test(_TestSubscriberLimit)

class \nodoc\ iso _TestHandleableSignalAccepts is UnitTest
  """
  Verify that the enumerated handleable signals pass HandleableSignalValidator.
  (The real-time ranges are pinned separately by the RT boundaries test.)
  """
  fun name(): String => "signals/HandleableSignal accepts handleable"

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
    elseif haiku then
      _assert_valid(h, Sig.hup())
      _assert_valid(h, Sig.int())
      _assert_valid(h, Sig.quit())
      _assert_valid(h, Sig.pipe())
      _assert_valid(h, Sig.alrm())
      _assert_valid(h, Sig.term())
      _assert_valid(h, Sig.urg())
      _assert_valid(h, Sig.tstp())
      _assert_valid(h, Sig.cont())
      _assert_valid(h, Sig.chld())
      _assert_valid(h, Sig.ttin())
      _assert_valid(h, Sig.ttou())
      _assert_valid(h, Sig.xcpu())
      _assert_valid(h, Sig.xfsz())
      _assert_valid(h, Sig.vtalrm())
      _assert_valid(h, Sig.prof())
      _assert_valid(h, Sig.winch())
      _assert_valid(h, Sig.usr1())
      _assert_valid(h, Sig.sys())
    elseif windows then
      _assert_valid(h, Sig.int())
      _assert_valid(h, Sig.term())
    end

  fun _assert_valid(h: TestHelper, sig: U32) =>
    match MakeHandleableSignal(sig)
    | let _: HandleableSignal => None
    | let _: ValidationFailure =>
      h.fail("signal " + sig.string() + " should be valid")
    end

class \nodoc\ iso _TestHandleableSignalRejectsFatal is UnitTest
  """
  Verify that fatal signals are rejected by HandleableSignalValidator.
  """
  fun name(): String => "signals/HandleableSignal rejects fatal"

  fun apply(h: TestHelper) =>
    ifdef linux or bsd or osx or haiku then
      _assert_invalid(h, Sig.ill())
      _assert_invalid(h, Sig.trap())
      _assert_invalid(h, Sig.abrt())
      _assert_invalid(h, Sig.fpe())
      _assert_invalid(h, Sig.bus())
      _assert_invalid(h, Sig.segv())
    elseif windows then
      _assert_invalid(h, Sig.abrt())
      // The numbers MSVC's CRT actually uses for its fatal set — SIGILL,
      // SIGFPE, SIGSEGV, SIGABRT — none of which have portable accessors
      // for these values.
      _assert_invalid(h, 4)
      _assert_invalid(h, 8)
      _assert_invalid(h, 11)
      _assert_invalid(h, 22)
    end

  fun _assert_invalid(h: TestHelper, sig: U32) =>
    match MakeHandleableSignal(sig)
    | let _: HandleableSignal =>
      h.fail("signal " + sig.string() + " should be rejected (fatal)")
    | let _: ValidationFailure => None
    end

class \nodoc\ iso _TestHandleableSignalRejectsUncatchable is UnitTest
  """
  Verify that uncatchable signals are rejected by HandleableSignalValidator.
  """
  fun name(): String => "signals/HandleableSignal rejects uncatchable"

  fun apply(h: TestHelper) =>
    ifdef linux or bsd or osx or haiku then
      _assert_invalid(h, Sig.kill())
      _assert_invalid(h, Sig.stop())
    elseif windows then
      _assert_invalid(h, Sig.kill())
    end

  fun _assert_invalid(h: TestHelper, sig: U32) =>
    match MakeHandleableSignal(sig)
    | let _: HandleableSignal =>
      h.fail("signal " + sig.string() + " should be rejected (uncatchable)")
    | let _: ValidationFailure => None
    end

class \nodoc\ iso _TestHandleableSignalRejectsUnknown is UnitTest
  """
  Verify that arbitrary unknown signal numbers are rejected.
  """
  fun name(): String => "signals/HandleableSignal rejects unknown"

  fun apply(h: TestHelper) =>
    _assert_invalid(h, 0)
    _assert_invalid(h, 200)
    _assert_invalid(h, U32.max_value())
    ifdef windows then
      // These accessors compile on Windows but name signals it does not
      // have; validation must reject them.
      _assert_invalid(h, Sig.hup())
      _assert_invalid(h, Sig.quit())
      _assert_invalid(h, Sig.alrm())
    end

  fun _assert_invalid(h: TestHelper, sig: U32) =>
    match MakeHandleableSignal(sig)
    | let _: HandleableSignal =>
      h.fail("signal " + sig.string() + " should be rejected (unknown)")
    | let _: ValidationFailure => None
    end

class \nodoc\ iso _TestHandleableSignalRejectsReserved is UnitTest
  """
  Verify that SIGUSR2 is rejected on the builds where the runtime reserves it
  as its scheduler sleep/wake signal — every build except
  scheduler_scaling_pthreads (which macOS forces). That reservation is what
  made the old system's SIGUSR2 handlers silently never fire, and the
  whitelist now refuses the number so the failure is visible instead. Sig.usr2()
  is a compile error on these builds, so the number is written out: 12 on
  Linux, 31 on BSD, 19 on Haiku.
  """
  fun name(): String => "signals/HandleableSignal rejects reserved"

  fun apply(h: TestHelper) =>
    ifdef not "scheduler_scaling_pthreads" then
      ifdef linux then
        _assert_invalid(h, 12)
      elseif bsd then
        _assert_invalid(h, 31)
      elseif haiku then
        _assert_invalid(h, 19)
      end
    end

  fun _assert_invalid(h: TestHelper, sig: U32) =>
    match MakeHandleableSignal(sig)
    | let _: HandleableSignal =>
      h.fail("SIGUSR2 (" + sig.string() + ") should be rejected (reserved)")
    | let _: ValidationFailure => None
    end

class \nodoc\ iso _TestHandleableSignalRTBoundaries is UnitTest
  """
  Pin the real-time signal ranges at their boundaries: the first and last
  in-range values validate and the values just outside do not. Uses Sig.rt
  for the in-range values so a desync between Sig.rt's range and
  HandleableSignalValidator's is caught from either side.
  """
  fun name(): String => "signals/HandleableSignal RT boundaries"

  fun apply(h: TestHelper) =>
    ifdef linux then
      try
        _assert_valid(h, Sig.rt(0)?)
        _assert_valid(h, Sig.rt(32)?)
      else
        h.fail("in-range Sig.rt errored")
      end
      _assert_invalid(h, 65)
    elseif bsd then
      try
        _assert_valid(h, Sig.rt(0)?)
        _assert_valid(h, Sig.rt(61)?)
      else
        h.fail("in-range Sig.rt errored")
      end
      _assert_invalid(h, 64)
      _assert_invalid(h, 127)
    elseif osx then
      // The RT range is bsd-only; it must not leak into the osx whitelist.
      _assert_invalid(h, 65)
    elseif haiku then
      try
        _assert_valid(h, Sig.rt(0)?)
        _assert_valid(h, Sig.rt(7)?)
      else
        h.fail("in-range Sig.rt errored")
      end
      _assert_invalid(h, 8)
    end

  fun _assert_valid(h: TestHelper, sig: U32) =>
    match MakeHandleableSignal(sig)
    | let _: HandleableSignal => None
    | let _: ValidationFailure =>
      h.fail("signal " + sig.string() + " should be valid")
    end

  fun _assert_invalid(h: TestHelper, sig: U32) =>
    match MakeHandleableSignal(sig)
    | let _: HandleableSignal =>
      h.fail("signal " + sig.string() + " should be rejected (out of range)")
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
  fun exclusion_group(): String => "signals"

  fun ref apply(h: TestHelper) =>
    let auth = SignalAuth(h.env.root)
    match MakeHandleableSignal(Sig.int())
    | let sig: HandleableSignal =>
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

class \nodoc\ iso _TestSignalUSR2 is UnitTest
  var _signal: (SignalHandler | None) = None

  fun name(): String => "signals/USR2"
  fun exclusion_group(): String => "signals"

  fun ref apply(h: TestHelper) =>
    // Gated to match the registration in tests() and the whitelist in
    // HandleableSignalValidator: the runtime frees SIGUSR2 only on
    // scheduler_scaling_pthreads builds, and it exists only off Windows, so
    // that is where Sig.usr2() yields a usable signal number.
    ifdef "scheduler_scaling_pthreads" and not windows then
      let auth = SignalAuth(h.env.root)
      match MakeHandleableSignal(Sig.usr2())
      | let sig: HandleableSignal =>
        let signal = SignalHandler(auth, _TestSignalNotify(h), sig)
        signal.raise(auth)
        _signal = signal
        h.long_test(10_000_000_000)
      | let _: ValidationFailure =>
        h.fail("SIGUSR2 should be a valid signal on this build")
      end
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

class \nodoc\ _TestMultiHandlerChainNotify is SignalNotify
  """
  On the first notification only, raises the signal again via the next
  handler, then completes its action. Serializing the second raise behind
  the first delivery keeps the raises from overlapping — on Windows the
  CRT resets the disposition to SIG_DFL before running a handler, so two
  concurrent raises leave a window where the second kills the process.
  """
  let _h: TestHelper
  let _action: String
  let _next: SignalHandler
  let _auth: SignalAuth
  var _chained: Bool = false

  new iso create(h: TestHelper, action: String, next: SignalHandler,
    auth: SignalAuth)
  =>
    _h = h
    _action = action
    _next = next
    _auth = auth

  fun ref apply(count: U32): Bool =>
    if not _chained then
      _chained = true
      // Same-actor ordering: _next's constructor (and thus its
      // subscription) completed before this raise runs on it. The raise
      // is sent BEFORE the action completes so it lands in _next's
      // mailbox ahead of any tear_down dispose that test completion
      // could trigger — a raise processed after every handler has
      // unsubscribed would hit the default disposition and kill the
      // process.
      _next.raise(_auth)
    end
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
  fun exclusion_group(): String => "signals"

  fun ref apply(h: TestHelper) =>
    let auth = SignalAuth(h.env.root)
    h.expect_action("handler1")
    h.expect_action("handler2")
    match MakeHandleableSignal(Sig.int())
    | let sig: HandleableSignal =>
      let s2 = SignalHandler(auth,
        _TestMultiHandlerNotify(h, "handler2"), sig)
      let s1 = SignalHandler(auth,
        _TestMultiHandlerChainNotify(h, "handler1", s2, auth), sig)
      // Same-actor message ordering guarantees s1's constructor (and thus
      // its subscription) has completed before this raise executes. s2's
      // subscription may or may not be in place for this first raise;
      // s1's notify raises again through s2 (see the chain notify), and
      // that second raise is ordered after s2's subscription, so both
      // actions complete.
      s1.raise(auth)
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

  fun ref tear_down(h: TestHelper) =>
    let auth = SignalAuth(h.env.root)
    try (_signal1 as SignalHandler).dispose(auth) end
    try (_signal2 as SignalHandler).dispose(auth) end

class \nodoc\ _TestDisposeNotify is SignalNotify
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref apply(count: U32): Bool =>
    true

  fun ref disposed() =>
    _h.complete(true)

class \nodoc\ iso _TestDispose is UnitTest
  """
  Verify that disposing a SignalHandler calls the notify's disposed method
  once the runtime has finished the unregistration.
  """

  fun name(): String => "signals/dispose"
  fun exclusion_group(): String => "signals"

  fun ref apply(h: TestHelper) =>
    let auth = SignalAuth(h.env.root)
    match MakeHandleableSignal(Sig.int())
    | let sig: HandleableSignal =>
      let signal = SignalHandler(auth, _TestDisposeNotify(h), sig)
      signal.dispose(auth)
      h.long_test(10_000_000_000)
    | let _: ValidationFailure =>
      h.fail("SIGINT should be a valid signal")
    end

  fun timed_out(h: TestHelper) =>
    h.fail("disposed callback was not called")
    h.complete(false)

class \nodoc\ _DispositionProbeNotify is SignalNotify
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref apply(count: U32): Bool =>
    true

  fun ref disposed() =>
    // disposed() fires after the runtime finished the unregistration, so
    // the OS-default disposition is already restored. signal() returns the
    // previous handler; SIG_DFL is the null pointer on every supported
    // platform, and re-installing it here is idempotent.
    let previous = @signal(Sig.term().i32(), USize(0))
    _h.assert_true(previous.is_null(),
      "disposing the last handler must restore the default disposition")
    _h.complete(true)

class \nodoc\ iso _TestDisposeRestoresDefaultDisposition is UnitTest
  """
  Verify the documented contract that once a signal's last handler is
  disposed, the signal takes the OS-default disposition again — observed by
  reading the disposition back rather than delivering the signal, which
  would kill the process for SIGTERM. If the last-subscriber teardown
  regresses, the probe sees the runtime's handler (epoll and Windows) or
  SIG_IGN (kqueue) instead of SIG_DFL. If SIGTERM registration itself
  broke, this test passes vacuously; registration and delivery are pinned
  by the other tests in this package. It shares SIGTERM with the
  subscriber-limit test — see the note at the top of this file on why the
  SIGTERM tests clean up fully before finishing.
  """

  fun name(): String => "signals/dispose restores default disposition"
  fun exclusion_group(): String => "signals"

  fun ref apply(h: TestHelper) =>
    let auth = SignalAuth(h.env.root)
    match MakeHandleableSignal(Sig.term())
    | let sig: HandleableSignal =>
      let signal = SignalHandler(auth, _DispositionProbeNotify(h), sig)
      signal.dispose(auth)
      h.long_test(10_000_000_000)
    | let _: ValidationFailure =>
      h.fail("SIGTERM should be a valid signal")
    end

  fun timed_out(h: TestHelper) =>
    h.fail("disposed callback was not called")
    h.complete(false)

class \nodoc\ _IgnoredDispositionProbeNotify is SignalNotify
  let _h: TestHelper
  let _signum: I32

  new iso create(h: TestHelper, signum: I32) =>
    _h = h
    _signum = signum

  fun ref apply(count: U32): Bool =>
    true

  fun ref disposed() =>
    // disposed() fires after the runtime finished the unregistration, so the
    // restored disposition is already in place. signal() returns the previous
    // disposition and installs the one passed; SIG_IGN is address 1, SIG_DFL
    // is 0. Passing SIG_IGN keeps the signal ignored, so the probe does not
    // reintroduce the SIG_DFL it is checking against.
    let previous = @signal(_signum, USize(1))
    _h.assert_eq[USize](USize(1), previous.usize(),
      "disposing the last handler must restore SIG_IGN, not SIG_DFL")
    _h.complete(true)

class \nodoc\ iso _TestDisposeRestoresIgnoredDisposition is UnitTest
  """
  Verify that disposing a signal's last handler restores the disposition the
  runtime had in place before the handler was registered, not a blanket
  SIG_DFL. SIGPIPE is the case that matters: the runtime ignores it
  (socket.c) so a write to a closed socket fails with an error instead of a
  signal, and socket.c invites a program to handle SIGPIPE through this
  package. Disposing that handler must return SIGPIPE to SIG_IGN; a
  regression to SIG_DFL would kill the process on the next write to a closed
  socket. The disposition is read back rather than raised. SIGPIPE is not
  handleable on Windows, so this runs off Windows.
  """
  fun name(): String => "signals/dispose restores ignored disposition"
  fun exclusion_group(): String => "signals"

  fun ref apply(h: TestHelper) =>
    ifdef not windows then
      let auth = SignalAuth(h.env.root)
      match MakeHandleableSignal(Sig.pipe())
      | let sig: HandleableSignal =>
        let signal = SignalHandler(auth,
          _IgnoredDispositionProbeNotify(h, Sig.pipe().i32()), sig)
        signal.dispose(auth)
        h.long_test(10_000_000_000)
      | let _: ValidationFailure =>
        h.fail("SIGPIPE should be a valid signal off Windows")
      end
    end

  fun timed_out(h: TestHelper) =>
    h.fail("disposed callback was not called")
    h.complete(false)

class \nodoc\ _TestReturnsFalseNotify is SignalNotify
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref apply(count: U32): Bool =>
    false

  fun ref disposed() =>
    _h.complete(true)

class \nodoc\ iso _TestNotifyReturnsFalse is UnitTest
  """
  Verify that a notify returning false auto-disposes the handler.
  """
  var _signal: (SignalHandler | None) = None

  fun name(): String => "signals/notify returns false"
  fun exclusion_group(): String => "signals"

  fun ref apply(h: TestHelper) =>
    let auth = SignalAuth(h.env.root)
    match MakeHandleableSignal(Sig.int())
    | let sig: HandleableSignal =>
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

class \nodoc\ _LimitFillNotify is SignalNotify
  let _h: TestHelper
  let _coordinator: _SubscriberLimitCoordinator
  let _n: USize
  var _fired: Bool = false

  new iso create(h: TestHelper, coordinator: _SubscriberLimitCoordinator,
    n: USize)
  =>
    _h = h
    _coordinator = coordinator
    _n = n

  fun ref apply(count: U32): Bool =>
    if count == 0 then
      _h.fail("signal delivered with a zero count")
    end
    if not _fired then
      _fired = true
      _h.complete_action("limit-h" + _n.string())
      _coordinator.registered()
    end
    true

  fun ref registration_failed(reason: SignalRegistrationError) =>
    _h.fail("fill notify " + _n.string() + " failed to register")

  fun ref disposed() =>
    if not _fired then
      _h.fail("fill notify " + _n.string() + " disposed without ever firing")
    end
    _coordinator.disposed_confirmed()

class \nodoc\ _LimitRejectNotify is SignalNotify
  let _h: TestHelper
  let _coordinator: _SubscriberLimitCoordinator
  var _rejected: Bool = false
  var _disposed: Bool = false

  new iso create(h: TestHelper, coordinator: _SubscriberLimitCoordinator) =>
    _h = h
    _coordinator = coordinator

  fun ref apply(count: U32): Bool =>
    _h.fail("rejected handler received a signal")
    true

  fun ref registration_failed(reason: SignalRegistrationError) =>
    if _rejected then
      _h.fail("registration_failed ran twice on the rejected handler")
      return
    end
    _rejected = true
    match reason
    | SignalSubscriberLimit =>
      _h.complete_action("limit-rejected")
      _coordinator.rejected()
    | SignalRegistrationRefused =>
      _h.fail("17th handler reported refused instead of the limit")
    end

  fun ref disposed() =>
    if not _rejected then
      _h.fail("rejected handler was disposed without registration_failed")
    end
    if _disposed then
      _h.fail("disposed ran twice on the rejected handler")
    else
      _disposed = true
      // Completing this only here keeps the auto-dispose half of the
      // failure contract load-bearing: registration_failed alone must not
      // pass the test.
      _h.complete_action("limit-rejected-disposed")
      _coordinator.disposed_confirmed()
    end

class \nodoc\ _LimitReuseNotify is SignalNotify
  let _h: TestHelper
  let _coordinator: _SubscriberLimitCoordinator
  var _fired: Bool = false

  new iso create(h: TestHelper, coordinator: _SubscriberLimitCoordinator) =>
    _h = h
    _coordinator = coordinator

  fun ref apply(count: U32): Bool =>
    if not _fired then
      _fired = true
      _coordinator.reuse_succeeded()
    end
    true

  fun ref registration_failed(reason: SignalRegistrationError) =>
    match reason
    | SignalSubscriberLimit =>
      // The freed slot's cancel hadn't been processed yet — try again.
      _coordinator.retry_reuse()
    | SignalRegistrationRefused =>
      // Refused is permanent; retrying would loop forever.
      _h.fail("reuse replacement was refused by the OS")
    end

  fun ref disposed() =>
    _coordinator.disposed_confirmed()

actor \nodoc\ _SubscriberLimitCoordinator
  """
  Drives the subscriber-limit test: fills the SIGTERM table one handler at a
  time (each confirmed by its first notification before the next is
  created), then triggers the 17th's rejection, then proves a disposed slot
  is reusable. Raises are serialized — at most one in flight — to stay clear
  of the Windows CRT one-shot re-arm window.
  """
  let _h: TestHelper
  let _auth: SignalAuth
  let _sig: HandleableSignal
  let _handlers: Array[SignalHandler] = Array[SignalHandler](18)
  var _registered: USize = 0
  var _stopped: Bool = false
  var _disposed_count: USize = 0
  var _quiesced: Bool = false

  new create(h: TestHelper, auth: SignalAuth, sig: HandleableSignal) =>
    _h = h
    _auth = auth
    _sig = sig
    _create_fill_handler()

  fun ref _create_fill_handler() =>
    let n = _handlers.size() + 1
    let s = SignalHandler(_auth, _LimitFillNotify(_h, this, n), _sig)
    _handlers.push(s)
    // Same-actor message ordering: the raise runs after the constructor, so
    // the subscription is registered before the signal fires.
    s.raise(_auth)

  fun ref _create_reuse_handler() =>
    let s = SignalHandler(_auth, _LimitReuseNotify(_h, this), _sig)
    _handlers.push(s)
    s.raise(_auth)

  be registered() =>
    if _stopped then return end
    _registered = _registered + 1
    if _registered < 16 then
      _create_fill_handler()
    elseif _registered == 16 then
      // All 16 slots are provably occupied (each fill handler's first
      // notification confirmed its registration, and nothing has
      // unsubscribed). Construction alone triggers the 17th's rejection.
      _handlers.push(
        SignalHandler(_auth, _LimitRejectNotify(_h, this), _sig))
    end

  be rejected() =>
    if _stopped then return end
    // Explicitly dispose the already-auto-disposed handler: a user who
    // doesn't know registration failed may do exactly this. With the
    // idempotence guard intact it is a no-op; a regression fires the
    // reject notify's second-dispose failure.
    try _handlers(16)?.dispose(_auth) end
    // Free one slot from the full table, then prove a new handler can
    // take it. A middle handler, not the first: freeing slot 0 would let a
    // degenerate always-remove-slot-0 bug pass by coincidence.
    try _handlers(5)?.dispose(_auth) end
    _create_reuse_handler()

  be retry_reuse() =>
    if _stopped then return end
    _create_reuse_handler()

  be reuse_succeeded() =>
    if _stopped then return end
    _h.complete_action("limit-slot-reused")
    // The test's assertions are done. Dispose every handler and wait for all
    // of them to finish unregistering (disposed_confirmed) before completing,
    // so this test leaves SIGTERM's disposition restored and no other SIGTERM
    // test depends on running after it.
    _dispose_all()

  be disposed_confirmed() =>
    // Each handler this test created — the 16 fills, the rejected 17th, and
    // every reuse attempt — reports here once its unregistration is complete.
    // When the last one does (and _dispose_all has run, so no more will be
    // created), SIGTERM has no subscribers left and its disposition is back to
    // what it was before the test.
    _disposed_count = _disposed_count + 1
    if _stopped and (not _quiesced) and (_disposed_count == _handlers.size())
    then
      _quiesced = true
      _h.complete_action("limit-quiesced")
    end

  be dispose_all() =>
    _dispose_all()

  fun ref _dispose_all() =>
    // Latch first: a retry_reuse queued behind this must not create-and-raise
    // after the table has been emptied — a raise with no subscribers takes the
    // default disposition and would kill the test process. (A replacement
    // created before this ran can still have a raise in flight. If that
    // replacement was rejected it is NOT registered; its raise is safe in
    // practice because the replacement processes the raise adjacent to its
    // rejection, long before the 16 serialized cancels below can drain the
    // table — and this window exists only on the timeout path, where the run
    // is already failing.)
    _stopped = true
    for s in _handlers.values() do
      s.dispose(_auth)
    end

class \nodoc\ iso _TestSubscriberLimit is UnitTest
  """
  Verify the 16-subscriber-per-signal limit: 16 handlers register and all
  receive the signal; a 17th is rejected (registration_failed with
  SignalSubscriberLimit, then dispose, with apply never run); disposing a
  subscriber frees its slot for a later handler.

  Uses SIGTERM: it is valid on every platform (including Windows). Before it
  completes it disposes every handler it created and waits for all of them to
  finish unregistering, so it leaves SIGTERM's disposition restored and no
  other SIGTERM test depends on running after it — see the note at the top of
  this file.
  """
  var _coordinator: (_SubscriberLimitCoordinator | None) = None

  fun name(): String => "signals/subscriber limit"
  fun exclusion_group(): String => "signals"

  fun ref apply(h: TestHelper) =>
    let auth = SignalAuth(h.env.root)
    match MakeHandleableSignal(Sig.term())
    | let sig: HandleableSignal =>
      // Declare every expected action before any handler exists so no
      // completion can be lost.
      var i: USize = 1
      while i <= 16 do
        h.expect_action("limit-h" + i.string())
        i = i + 1
      end
      h.expect_action("limit-rejected")
      h.expect_action("limit-rejected-disposed")
      h.expect_action("limit-slot-reused")
      h.expect_action("limit-quiesced")
      _coordinator = _SubscriberLimitCoordinator(h, auth, sig)
      h.long_test(10_000_000_000)
    | let _: ValidationFailure =>
      h.fail("SIGTERM should be a valid signal")
    end

  fun timed_out(h: TestHelper) =>
    try (_coordinator as _SubscriberLimitCoordinator).dispose_all() end
    h.fail("timeout")
    h.complete(false)

  fun ref tear_down(h: TestHelper) =>
    try (_coordinator as _SubscriberLimitCoordinator).dispose_all() end

class \nodoc\ _RefusedNotify is SignalNotify
  let _h: TestHelper
  let _action: String
  var _failed: Bool = false

  new iso create(h: TestHelper, action: String) =>
    _h = h
    _action = action

  fun ref apply(count: U32): Bool =>
    _h.fail("handler for an OS-refused signal received a signal")
    true

  fun ref registration_failed(reason: SignalRegistrationError) =>
    match reason
    | SignalRegistrationRefused =>
      _failed = true
    | SignalSubscriberLimit =>
      _h.fail("OS-refused registration reported the subscriber limit")
    end

  fun ref disposed() =>
    // Completing only here keeps the auto-dispose half of the failure
    // contract load-bearing: registration_failed alone must not pass.
    if _failed then
      _h.complete_action(_action)
    else
      _h.fail("refused handler was disposed without registration_failed")
    end

class \nodoc\ iso _TestOSRefusedRegistration is UnitTest
  """
  Verify the documented contract for registrations the OS refuses: glibc
  and musl both reject sigaction for real-time signal 32 (reserved for the
  libc's threading internals) even though the whitelist admits it; the
  notify must get registration_failed with SignalRegistrationRefused and
  then dispose, with apply never run. The second handler proves the
  failure path resets the registration state — a regression that left it
  mid-install would strand the second subscribe and surface here as a
  timeout.
  """
  fun name(): String => "signals/OS-refused registration auto-disposes"
  fun exclusion_group(): String => "signals"

  fun ref apply(h: TestHelper) =>
    ifdef linux then
      let auth = SignalAuth(h.env.root)
      try
        match MakeHandleableSignal(Sig.rt(0)?)
        | let sig: HandleableSignal =>
          h.expect_action("refused-1")
          h.expect_action("refused-2")
          SignalHandler(auth, _RefusedNotify(h, "refused-1"), sig)
          SignalHandler(auth, _RefusedNotify(h, "refused-2"), sig)
          h.long_test(10_000_000_000)
        | let _: ValidationFailure =>
          h.fail("rt(0) should validate")
        end
      else
        h.fail("Sig.rt(0) errored")
      end
    end

  fun timed_out(h: TestHelper) =>
    h.fail("timeout")
    h.complete(false)
