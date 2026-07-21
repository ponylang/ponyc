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

    Delivery batching differs by platform: on Linux, macOS, and the BSDs
    several firings that arrive close together can be reported in one call
    with `count` greater than one, while on Windows each firing is a
    separate call with a `count` of one.
    """
    true

  fun ref registration_failed(reason: SignalRegistrationError) =>
    """
    Called when the handler's registration could not be completed.
    `SignalSubscriberLimit` is transient — a slot may open once another
    subscriber unsubscribes; `SignalRegistrationRefused` is permanent, so
    re-registering the same signal will fail again. The handler is then
    disposed — `disposed` follows once the runtime confirms it, and `apply`
    will not have run. If the handler was explicitly disposed before the
    failure was delivered, only `disposed` is called.
    """
    None

  fun ref disposed() =>
    """
    Called when the runtime has finished unregistering the handler,
    whichever way disposal began: explicitly via `SignalHandler.dispose`,
    when `apply` returned false, or when the registration could not be
    completed (preceded by `registration_failed` in that case). `apply`
    will never be called after this.

    By the time this is called, the handler no longer affects signal
    delivery: if it was the signal's last subscriber, the signal's
    disposition from before it was first handled has been restored. For
    most signals that is the operating system default, so raising a
    terminating one from here ends the process; for a signal the runtime
    keeps ignored, such as SIGPIPE, it is SIG_IGN. A handler disposed while
    the runtime itself is shutting down may never receive this call.
    """
    None

primitive SignalRaise
  """
  Raise a signal.

  Unlike SignalHandler, this accepts a raw signal number rather than a
  HandleableSignal. Raising fatal signals (e.g. SIGABRT to intentionally crash)
  is a legitimate operation — it is only handling them via the ASIO
  mechanism that is prevented.

  The raise is process-wide: every currently subscribed handler for the
  signal is notified. With no subscribers, the signal's current disposition
  applies — the operating system default for most signals (process death for
  a terminating one), but SIG_IGN for one the runtime keeps ignored like
  SIGPIPE.
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
