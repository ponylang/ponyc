interface StdinNotify
  """
  Notification for data arriving via stdin.
  """
  fun ref apply(data: Array[U8] iso) =>
    """
    Called when data is available on stdin.
    """
    None

  fun ref dispose() =>
    """
    Called when no more data will arrive on stdin.
    """
    None

interface DisposeableActor tag
  """
  An interface used to asynchronously dispose of an actor.
  """
  be dispose()

actor Stdin
  """
  Asynchronous access to stdin. The constructor is private to ensure that
  access is provided only via an environment.
  """
  var _notify: (StdinNotify | None) = None
  var _event: AsioEventID = AsioEvent.none()
  let _use_event: Bool

  new _create(use_event: Bool) =>
    """
    Create an asynchronous stdin provider.
    """
    _use_event = use_event

  be apply(notify: (StdinNotify iso | None)) =>
    """
    Set the notifier.
    """
    _set_notify(consume notify)

  be dispose() =>
    """
    Clear the notifier in order to shut down input.
    """
    _set_notify(None)

  fun ref _set_notify(notify: (StdinNotify iso | None)) =>
    """
    Set the notifier.
    """
    if notify is None then
      if _use_event and not _event.is_null() then
        // Unsubscribe the event.
        @asio_event_unsubscribe[None](_event)
        _event = AsioEvent.none()
      end
    elseif _notify is None then
      if _use_event then
        // Create a new event.
        _event = @asio_event_create[AsioEventID](this, U64(0), U32(1), true)
      else
        // Start the read loop.
        _loop_read()
      end
    end

    try (_notify as StdinNotify).dispose() end
    _notify = consume notify

  be _loop_read() =>
    """
    If we are able to read from stdin, schedule another read.
    """
    if _read() then
      _loop_read()
    end

  be _event_notify(event: AsioEventID, flags: U32, arg: U64) =>
    """
    When the event fires, read from stdin.
    """
    if AsioEvent.disposable(flags) then
      @asio_event_destroy[None](this, event)
    elseif (_event is event) and AsioEvent.readable(flags) then
      _read()
    end

  be _read_again() =>
    """
    Resume reading.
    """
    _read()

  fun ref _read(): Bool =>
    """
    Read a chunk of data from stdin. If we read 4 kb of data, send ourself a
    resume message and stop reading, to avoid starving other actors.
    """
    try
      let notify = _notify as StdinNotify
      var sum: U64 = 0

      while true do
        var len = U64(64)
        var data = recover Array[U8].undefined(len) end
        var again: Bool = false
        len = @os_stdin_read[U64](data.cstring(), data.space(), &again)

        match len
        | -1 =>
          // Error, possibly would block. Try again.
          return true
        | 0 =>
          // EOF. Close everything, stop reading.
          _close_event()
          notify.dispose()
          _notify = None
          return false
        end

        data.truncate(len)
        notify(consume data)

        if not again then
          // Not allowed to call os_stdin_read again yet, exit loop.
          return true
        end

        sum = sum + len

        if sum > (1 << 12) then
          if _use_event then
            _read_again()
          end

          break
        end
      end
      true
    else
      // No notifier. Stop reading.
      _close_event()
      false
    end

    fun ref _close_event() =>
      """
      Close the event.
      """
      if not _event.is_null() then
        @asio_event_unsubscribe[None](_event)
        _event = AsioEvent.none()
      end
