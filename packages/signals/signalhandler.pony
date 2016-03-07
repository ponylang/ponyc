use @pony_asio_event_create[AsioEventID](owner: AsioEventNotify, fd: U32,
  flags: U32, nsec: U64, noisy: Bool)
use @pony_asio_event_unsubscribe[None](event: AsioEventID)
use @pony_asio_event_destroy[None](event: AsioEventID)

actor SignalHandler
  """
  Listen for a specific signal.
  """
  let _notify: SignalNotify
  let _sig: U32
  var _event: AsioEventID

  new create(notify: SignalNotify iso, sig: U32) =>
    """
    Create a signal handler.
    """
    _notify = consume notify
    _sig = sig
    _event =
      @pony_asio_event_create(this, 0, AsioEvent.signal(), sig.u64(), false)

  be raise() =>
    """
    Raise the signal.
    """
    SignalRaise(_sig)

  be dispose() =>
    """
    Dispose of the signal handler.
    """
    _dispose()

  be _event_notify(event: AsioEventID, flags: U32, arg: U32) =>
    """
    Called when the signal is received, or when the AsioEventID can be
    destroyed.
    """
    if AsioEvent.disposable(flags) then
      @pony_asio_event_destroy(event)
    elseif event is _event then
      if not _notify(arg) then
        _dispose()
      end
    end

  fun ref _dispose() =>
    """
    Dispose of the AsioEventID.
    """
    if not _event.is_null() then
      @pony_asio_event_unsubscribe(_event)
      _event = AsioEvent.none()
      _notify.dispose()
    end
