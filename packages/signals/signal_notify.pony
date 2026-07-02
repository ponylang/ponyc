use @getpid[I32]()
use @kill[I32](pid_t: I32, sig: U32)
use @raise[I32](sig: U32)

primitive SignalSubscriberLimit
  """
  The per-signal subscriber limit (16) was already reached. A later
  registration for the same signal can succeed once a current subscriber
  unsubscribes.
  """

primitive SignalRegistrationRefused
  """
  The operating system refused the registration. Retrying will not
  succeed.
  """

type SignalRegistrationError is
  (SignalSubscriberLimit | SignalRegistrationRefused)
  """
  Why a `SignalHandler`'s registration could not be completed.
  """

interface SignalNotify
  """
  Notifications for a signal.
  """
  fun ref apply(count: U32): Bool =>
    """
    Called with the number of times the signal has fired since this was
    last called. Return false to stop listening for the signal.
    """
    true

  fun ref registration_failed(reason: SignalRegistrationError) =>
    """
    Called when the handler's registration could not be completed.
    `SignalSubscriberLimit` is transient — a slot may open once another
    subscriber unsubscribes; `SignalRegistrationRefused` is permanent, so
    re-registering the same signal will fail again. The handler is then
    disposed — `dispose` is called immediately after this, and `apply`
    will not have run. If the handler was explicitly disposed before the
    failure was delivered, only `dispose` is called.
    """
    None

  fun ref dispose() =>
    """
    Called when the signal handler is disposed: explicitly via
    `SignalHandler.dispose`, when `apply` returns false, or when the
    handler's registration could not be completed (preceded by
    `registration_failed` in that case).
    """
    None

primitive SignalRaise
  """
  Raise a signal.

  Unlike SignalHandler, this accepts a raw signal number rather than a
  ValidSignal. Raising fatal signals (e.g. SIGABRT to intentionally crash)
  is a legitimate operation — it is only handling them via the ASIO
  mechanism that is prevented.

  The raise is process-wide: every currently subscribed handler for the
  signal is notified. With no subscribers, the operating system's default
  disposition applies — for most terminating signals, process death.
  """
  fun apply(auth: SignalAuth, sig: U32) =>
    ifdef osx then
      // On Darwin, @raise delivers the signal to the current thread, not the
      // process, but kqueue EVFILT_SIGNAL will only see signals delivered to
      // the process. @kill delivers the signal to a specific process.
      @kill(@getpid(), sig)
    else
      @raise(sig)
    end
