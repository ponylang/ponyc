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
    test(_TestSubscriberLimit)

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
    elseif windows then
      _assert_valid(h, Sig.int())
      _assert_valid(h, Sig.term())
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
    elseif windows then
      _assert_invalid(h, Sig.abrt())
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
    elseif windows then
      _assert_invalid(h, Sig.kill())
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

class \nodoc\ _TestMultiHandlerChainNotify is SignalNotify
  """
  Completes its action and, on the first notification only, raises the
  signal again via the next handler. Serializing the second raise behind
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

  fun ref apply(h: TestHelper) =>
    let auth = SignalAuth(h.env.root)
    h.expect_action("handler1")
    h.expect_action("handler2")
    match MakeValidSignal(Sig.int())
    | let sig: ValidSignal =>
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
    if not _fired then
      _fired = true
      _h.complete_action("limit-h" + _n.string())
      _coordinator.registered()
    end
    true

  fun ref dispose() =>
    if not _fired then
      _h.fail("fill notify " + _n.string() + " disposed without ever firing")
    end

class \nodoc\ _LimitRejectNotify is SignalNotify
  let _h: TestHelper
  let _coordinator: _SubscriberLimitCoordinator
  var _disposed: Bool = false

  new iso create(h: TestHelper, coordinator: _SubscriberLimitCoordinator) =>
    _h = h
    _coordinator = coordinator

  fun ref apply(count: U32): Bool =>
    _h.fail("rejected handler received a signal")
    true

  fun ref dispose() =>
    if _disposed then
      _h.fail("dispose ran twice on the rejected handler")
    else
      _disposed = true
      _h.complete_action("limit-rejected")
      _coordinator.rejected()
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
      _h.complete_action("limit-slot-reused")
    end
    true

  fun ref dispose() =>
    // Before ever firing, dispose means this replacement was rejected — the
    // freed slot's cancel hadn't been processed yet — so ask for another
    // try. After a successful fire this is the legitimate cleanup dispose:
    // do nothing.
    if not _fired then
      _coordinator.retry_reuse()
    end

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
  let _sig: ValidSignal
  let _handlers: Array[SignalHandler] = Array[SignalHandler](18)
  var _registered: USize = 0
  var _stopped: Bool = false

  new create(h: TestHelper, auth: SignalAuth, sig: ValidSignal) =>
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
    // take it.
    try _handlers(0)?.dispose(_auth) end
    _create_reuse_handler()

  be retry_reuse() =>
    if _stopped then return end
    _create_reuse_handler()

  be dispose_all() =>
    // Latch first: a retry_reuse queued behind this message must not
    // create-and-raise after the table has been emptied — a raise with no
    // subscribers takes the default disposition and would kill the test
    // process. (A replacement created before this message was processed
    // can still have a raise in flight. If that replacement was rejected
    // it is NOT registered; its raise is safe in practice because the
    // replacement processes the raise adjacent to its rejection, long
    // before the 16 serialized cancels below can drain the table — and
    // this window exists only on the timeout path, where the run is
    // already failing.)
    _stopped = true
    for s in _handlers.values() do
      s.dispose(_auth)
    end

class \nodoc\ iso _TestSubscriberLimit is UnitTest
  """
  Verify the 16-subscriber-per-signal limit: 16 handlers register and all
  receive the signal; a 17th is rejected and auto-disposed (its notify's
  dispose runs, its apply never does); disposing a subscriber frees its
  slot for a later handler.

  Uses SIGTERM: it is valid on every platform (including Windows) and no
  other test subscribes to it, so the subscriber table starts empty. The
  table drains asynchronously after this test — any future SIGTERM test
  must tolerate residual occupancy.
  """
  var _coordinator: (_SubscriberLimitCoordinator | None) = None

  fun name(): String => "signals/subscriber limit"

  fun ref apply(h: TestHelper) =>
    let auth = SignalAuth(h.env.root)
    match MakeValidSignal(Sig.term())
    | let sig: ValidSignal =>
      // Declare every expected action before any handler exists so no
      // completion can be lost.
      var i: USize = 1
      while i <= 16 do
        h.expect_action("limit-h" + i.string())
        i = i + 1
      end
      h.expect_action("limit-rejected")
      h.expect_action("limit-slot-reused")
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
