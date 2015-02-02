interface StdinNotify
  """
  Notification for data arriving via stdin.
  """
  fun ref apply(data: Array[U8] iso): Bool =>
    """
    Called when data is available on stdin.
    """
    true

  fun ref closed() =>
    """
    Called when no more data will arrive on stdin.
    """
    None

actor Stdin
  """
  Asynchronous access to stdin. The constructor is private to ensure that
  access is provided only via an environment.
  """
  var _notify: (StdinNotify | None) = None
  var _event: Pointer[Event] tag = Pointer[Event]
  let _use_event: Bool

  new _create(use_event: Bool) =>
    """
    Create an asynchronous stdin provider.
    """
    _use_event = use_event

  be apply(notify: (StdinNotify iso | None)) =>
    """
    Replace the notifier.
    """
    if notify is None then
      if _use_event then
        // Unsubscribe the event.
        Event.unsubscribe(_event)
      end
    elseif _notify is None then
      if _use_event then
        // Create a new event.
        _event = Event._stdin(this)
      else
        // Start the read loop.
        _loop_read()
      end
    end

    _notify = consume notify

  be _loop_read() =>
    """
    If we are able to read from stdin, schedule another read.
    """
    if _read() then
      _loop_read()
    end

  be _event_notify(event: Pointer[Event] tag, flags: U32) =>
    """
    When the event fires, read from stdin.
    """
    if Event.disposable(flags) then
      Event.dispose(event)
    elseif (_event is event) and Event.readable(flags) then
      _read()
    end

  fun ref _read(): Bool =>
    """
    Read a chunk of data from stdin.
    """
    try
      let notify = _notify as StdinNotify
      var len = U64(64)
      var data = recover Array[U8].undefined(len) end
      len = @read[U64](U64(0), data.cstring(), data.space())

      match len
      | -1 =>
        // Error, possibly would block. Try again.
        true
      | 0 =>
        // EOF. Close everything, stop reading.
        Event.unsubscribe(_event)
        notify.closed()
        _notify = None
        false
      else
        data.truncate(len)

        if not notify(consume data) then
          // Notifier is done. Close everything, stop reading.
          Event.unsubscribe(_event)
          _notify = None
          false
        else
          true
        end
      end
    else
      // No notifier. Stop reading.
      Event.unsubscribe(_event)
      false
    end
