use @getpid[I32]()
use @kill[I32](pid_t: I32, sig: U32)
use @raise[I32](sig: U32)

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

  fun ref dispose() =>
    """
    Called when the signal handler is disposed: explicitly via
    `SignalHandler.dispose`, when `apply` returns false, or when the
    handler's registration could not be completed (the per-signal
    subscriber limit was reached, or the operating system refused the
    registration) — in that case `dispose` arrives without `apply`
    having run.
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
