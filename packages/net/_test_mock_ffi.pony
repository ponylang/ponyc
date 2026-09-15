use @malloc[Pointer[AsioEvent] tag](size: USize)

primitive \nodoc\ _AsioErrorFlag
  """
  The ASIO error flag value (`1 << 4`), matching `AsioEvent.errored()`.
  `AsioEvent` has no flag constructor for errors — only the `errored`
  predicate — so this names the bit for mock backends that need to
  inject an error event.
  """
  fun apply(): U32 => 1 << 4

primitive \nodoc\ _SyntheticEvent
  """
  Allocate a synthetic `AsioEventID` — a distinct heap address for identity
  comparison. The pointer is never dereferenced; it serves only as a unique
  key for `event is _event` checks in `_event_notify`. Mock
  `AsioBackend.destroy()` is a no-op; the allocations are tiny and
  short-lived test infrastructure.
  """
  fun apply(): AsioEventID =>
    @malloc(USize(32))
