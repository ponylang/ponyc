use @pony_asio_event_create[AsioEventID](owner: AsioEventNotify, fd: U32,
  flags: U32, nsec: U64, noisy: Bool)
use @pony_asio_event_unsubscribe[None](event: AsioEventID)
use @pony_asio_event_destroy[None](event: AsioEventID)

type AsioEventID is Pointer[AsioEvent] tag

interface tag AsioEventNotify
  be _event_notify(event: AsioEventID, flags: U32, arg: U32)

primitive AsioEvent
  """
  Functions for asynchronous event notification.
  """

  fun create_event(
    owner: AsioEventNotify,
    fd: U32,
    flags: U32,
    nsec: U64,
    noisy: Bool)
    : AsioEventID
  =>
    @pony_asio_event_create(owner, fd, flags, nsec, noisy)

  fun unsubscribe(event: AsioEventID) =>
    @pony_asio_event_unsubscribe(event)

  fun destroy(event: AsioEventID) =>
    @pony_asio_event_destroy(event)

  fun none(): AsioEventID =>
    """
    An empty event.
    """
    AsioEventID

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

  fun oneshotable(flags: U32): Bool =>
    """
    Returns true if the flags contain the oneshot flag.
    """
    (flags and (1 << 8)) != 0

  fun dispose(): U32 => 0

  fun read(): U32 => 1 << 0

  fun write(): U32 => 1 << 1

  fun timer(): U32 => 1 << 2

  fun signal(): U32 => 1 << 3

  fun read_write(): U32 => read() or write()

  fun oneshot(): U32 => 1 << 8

  fun read_write_oneshot(): U32 => read() or write() or oneshot()
