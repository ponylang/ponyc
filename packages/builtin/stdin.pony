use @pony_asio_event_create[AsioEventID](owner: AsioEventNotify, fd: U32,
  flags: U32, nsec: U64, noisy: Bool)
use @pony_asio_event_unsubscribe[None](event: AsioEventID)
use @pony_asio_event_destroy[None](event: AsioEventID)
use @pony_os_stdin_read[USize](buffer: Pointer[U8] tag, size: USize)

interface InputNotify
  """
  Notification for data arriving via an input stream.
  """
  fun ref apply(data: Array[U8] iso) =>
    """
    Called when data is available on the stream.
    """
    None

  fun ref dispose() =>
    """
    Called when no more data will arrive on the stream.
    """
    None

interface tag InputStream
  """
  Asynchronous access to some input stream.
  """
  be apply(notify: (InputNotify iso | None), chunk_size: USize = 32)
    """
    Set the notifier. Optionally, also sets the chunk size, dictating the
    maximum number of bytes of each chunk that will be passed to the notifier.
    """

  be dispose() =>
    """
    Clear the notifier in order to shut down input.
    """
    None

actor Stdin is AsioEventNotify
  """
  Asynchronous access to stdin. The constructor is private to ensure that
  access is provided only via an environment.

  Reading from stdin is done by registering an `InputNotify`:

  ```pony
  actor Main
    new create(env: Env) =>
      // do not forget to call `env.input.dispose` at some point
      env.input(
        object iso is InputNotify
          fun ref apply(data: Array[U8] iso) =>
            env.out.write(String.from_iso_array(consume data))

          fun ref dispose() =>
            env.out.print("Done.")
        end,
        512)
  ```

  **Note:** For reading user input from a terminal, use the [term](term--index.md) package.
  """
  var _notify: (InputNotify | None) = None
  var _chunk_size: USize = 32
  var _event: AsioEventID = AsioEvent.none()
  let _use_event: Bool

  new _create(use_event: Bool) =>
    """
    Create an asynchronous stdin provider.
    """
    _use_event = use_event

  be apply(notify: (InputNotify iso | None), chunk_size: USize = 32) =>
    """
    Set the notifier. Optionally, also sets the chunk size, dictating the
    maximum number of bytes of each chunk that will be passed to the notifier.
    """
    _set_notify(consume notify)
    _chunk_size = chunk_size

  be dispose() =>
    """
    Clear the notifier in order to shut down input.
    """
    _set_notify(None)

  fun ref _set_notify(notify: (InputNotify iso | None)) =>
    """
    Set the notifier.
    """
    if notify is None then
      if _use_event and not _event.is_null() then
        // Unsubscribe the event.
        @pony_asio_event_unsubscribe(_event)
        _event = AsioEvent.none()
      end
    elseif _notify is None then
      if _use_event then
        // Create a new event.
        _event = @pony_asio_event_create(this, 0, AsioEvent.read(), 0, true)
      else
        // Start the read loop.
        _loop_read()
      end
    end

    try (_notify as InputNotify).dispose() end
    _notify = consume notify

  be _loop_read() =>
    """
    If we are able to read from stdin, schedule another read.
    """
    if _read() then
      _loop_read()
    end

  be _event_notify(event: AsioEventID, flags: U32, arg: U32) =>
    """
    When the event fires, read from stdin.
    """
    if AsioEvent.disposable(flags) then
      @pony_asio_event_destroy(event)
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
    Read a chunk of data from stdin. Read a maximum of _chunk_size bytes, send
    ourself a resume message and stop reading to avoid starving other actors.
    """
    try
      let notify = _notify as InputNotify
      var sum: USize = 0

      while true do
        let chunk_size = _chunk_size
        var data = recover Array[U8] .> undefined(chunk_size) end

        let len = @pony_os_stdin_read(data.cpointer(), data.size())

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

        ifdef windows then
          // Not allowed to call pony_os_stdin_read again yet, exit loop.
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
        @pony_asio_event_unsubscribe(_event)
        _event = AsioEvent.none()
      end
