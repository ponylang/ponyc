use @pony_asio_event_create[AsioEventID](
  owner: AsioEventNotify,
  fd: U32,
  flags: U32,
  nsec: U64,
  noisy: Bool)
use @pony_asio_event_unsubscribe[None](event: AsioEventID)
use @pony_asio_event_destroy[None](event: AsioEventID)

actor SignalHandler is AsioEventNotify
  """
  Listen for a specific signal.

  Multiple SignalHandlers can be registered for the same signal — up to 16
  per signal number. All registered handlers will be notified when the
  signal is received, in no particular order. If a handler cannot be
  registered (the 16-subscriber limit is reached, or the runtime fails to
  register with the operating system), its notify's `registration_failed`
  is called with the reason and the handler is automatically disposed;
  `apply` will not have run.

  If the wait parameter is true, the program will not terminate until
  the SignalHandler's dispose method is called or the SignalNotify
  returns false after handling the signal. Disposing a SignalHandler
  unsubscribes it from the signal and is required to allow the runtime
  to garbage collect the handler.
  """
  let _notify: SignalNotify
  let _sig: U32
  var _event: AsioEventID

  new create(auth: SignalAuth, notify: SignalNotify iso, sig: HandleableSignal,
    wait: Bool = false)
  =>
    """
    Create a signal handler for a validated signal number.
    """
    _notify = consume notify
    _sig = sig()
    _event =
      @pony_asio_event_create(this, 0, AsioEvent.signal(), _sig.u64(), wait)

  be raise(auth: SignalAuth) =>
    """
    Raise the signal, process-wide: every currently subscribed handler for
    this signal is notified, not just this one. If no handler is subscribed
    when the signal is delivered — every handler was disposed, or none ever
    registered — the signal's current disposition applies: the operating
    system default for most signals, which ends the process for a terminating
    one, but SIG_IGN for a signal the runtime keeps ignored such as SIGPIPE,
    which does nothing.
    """
    SignalRaise(auth, _sig)

  be dispose(auth: SignalAuth) =>
    """
    Dispose of the signal handler, unsubscribing from the signal.
    """
    _dispose()

  be _event_notify(event: AsioEventID, flags: U32, arg: U32) =>
    """
    Called when the signal is received, when registration fails (the
    event arrives errored), or when the AsioEventID can be destroyed.
    """
    if AsioEvent.disposable(flags) then
      // The runtime sends this only after it has finished the
      // unregistration (the backends commit the teardown before the
      // disposal notification), so this is the moment `disposed` means:
      // the handler no longer affects signal delivery.
      @pony_asio_event_destroy(event)
      _notify.disposed()
    elseif (event is _event) and AsioEvent.errored(flags) then
      // The runtime reports why registration failed in the event's arg:
      // 1 = subscriber limit, anything else = the OS refused. The ASIO
      // backends define these codes (SIGNAL_ERR_* in epoll.c, kqueue.c, and
      // sock_notify.c); they and this mapping must stay in step.
      _notify.registration_failed(
        if arg == 1 then SignalSubscriberLimit
        else SignalRegistrationRefused end)
      _dispose()
    elseif event is _event then
      if not _notify(arg) then
        _dispose()
      end
    end

  fun ref _dispose() =>
    """
    Dispose of the AsioEventID. The notify's `disposed` is not called here:
    it fires when the runtime confirms the unregistration, in the
    disposable arm of `_event_notify`.
    """
    if not _event.is_null() then
      @pony_asio_event_unsubscribe(_event)
      _event = AsioEvent.none()
    end
