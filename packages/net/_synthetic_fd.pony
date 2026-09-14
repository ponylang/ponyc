primitive \nodoc\ _SyntheticFd
  """
  Sentinel fd for mock tests. `U32.max_value()` is not a valid POSIX
  file descriptor, so it cannot collide with a real open fd. Mock
  `TCPBackend.close` must be a no-op for this value.
  """
  fun apply(): U32 => U32.max_value()
