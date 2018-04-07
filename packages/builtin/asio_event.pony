use @pony_asio_event_alloc[AsioEventID](
  owner: AsioEventNotify,
  flags: U32,
  noisy: Bool)
use @pony_asio_event_set_nsec[None](event: AsioEventID, nsec: U64)
use @pony_asio_event_set_fd[None](event: AsioEventID, fd: U32)
use @pony_asio_event_set_signal[None](event: AsioEventID, sig: U32)
use @pony_asio_event_subscribe[None](event: AsioEventID)

type AsioEventID is Pointer[AsioEvent] tag

interface tag AsioEventNotify
  be _event_notify(event: AsioEventID, flags: U32, arg: U32)

primitive AsioEvent
  """
  Functions for asynchronous event notification.
  """
  fun none(): AsioEventID =>
    """
    An empty event.
    """
    AsioEventID

  fun make(owner: AsioEventNotify, flags: U32, fd: U32 = 0,
    nsec: U64 = 0, noisy: Bool = true, sig: U32 = 0): AsioEventID =>
    """
      Creates an ASIO event based on the provided parameters
    """
    let event = @pony_asio_event_alloc(owner, flags, noisy)
    if (nsec != 0) then
      @pony_asio_event_set_nsec(event, nsec)
    end
    if (fd != 0) then
      @pony_asio_event_set_fd(event, fd)
    end
    if (sig != 0) then
      @pony_asio_event_set_signal(event, sig)
    end
    @pony_asio_event_subscribe(event)
    event

  fun readable(flags: U32): Bool =>
    """
    Returns true if the flags contain the readable flag.
    """
    (flags and (1 << 0)) != 0

  fun writeable(flags: U32): Bool =>
    """
    Returns true if the flags contain the writeable flag.
    """
    (flags and (1 << 1)) != 0

  fun disposable(flags: U32): Bool =>
    """
    Returns true if the event should be disposed of.
    """
    flags == 0

  fun dispose(): U32 => 0

  fun read(): U32 => 1 << 0

  fun write(): U32 => 1 << 1

  fun timer(): U32 => 1 << 2

  fun signal(): U32 => 1 << 3

  fun read_write(): U32 => read() or write()

  fun oneshot(): U32 => 1 << 8

  fun read_write_oneshot(): U32 => read() or write() or oneshot()
