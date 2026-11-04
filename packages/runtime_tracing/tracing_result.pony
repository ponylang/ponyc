type TracingResult is
  ( TracingOk
  | TracingNotAvailable
  | TracingLogFileError )
  """
  The outcome of a tracing category operation.
  """

primitive TracingOk
  """
  The operation succeeded.
  """

primitive TracingNotAvailable
  """
  The caller is not running on a scheduler thread. Tracing category operations
  must be called from within a running actor.
  """

primitive TracingLogFileError
  """
  The trace output file could not be opened. Check that the path given to
  `--ponytracingoutput` is writable.
  """
