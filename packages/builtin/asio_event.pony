type AsioEventID is Pointer[AsioEvent] tag

trait tag AsioEventNotify
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

  fun errored(flags: U32): Bool =>
    """
    Returns true if the flags contain the error flag, indicating a
    subscription failure. The event is compromised and the actor should
    tear down.
    """
    (flags and (1 << 4)) != 0

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

  fun proc(): U32 =>
    """
    Subscribe for a process-exit event, which fires when the child exits. On
    kqueue platforms the event's fd is the child's pid; on Windows its `nsec`
    carries the child's process handle and its fd is unused. Must match
    `ASIO_PROC` in the runtime's `asio.h`.
    """
    1 << 5

  fun pipe(): U32 =>
    """
    Mark an event as a pipe that the Windows readiness backend peeks. A Windows
    pipe has no readiness signal, so the backend peeks it and delivers
    `ASIO_READ`/`ASIO_WRITE`. Combine with one direction flag (`read` or
    `write`). Must match `ASIO_PIPE` in the runtime's `asio.h`.
    """
    1 << 6

  fun read_write(): U32 => read() or write()

  fun oneshot(): U32 => 1 << 8

  fun read_write_oneshot(): U32 => read() or write() or oneshot()
