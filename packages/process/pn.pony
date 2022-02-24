interface ProcessNotify2
  """
  Notifications for Process connections.
  """

  fun ref created(process: _ProcessMonitorStateHandler ref) =>
    """
    ProcessMonitor calls this when it is created.
    """

  fun ref failed(process: _ProcessMonitorStateHandler ref, err: ProcessError) =>
    """
    ProcessMonitor calls this if we run into errors communicating with the
    forked process.
    """

// TODO: I dont like the existence of this
class ConsumedProcessNotify2
  fun ref created(process: _ProcessMonitorStateHandler ref) =>
    None

  fun ref failed(process: _ProcessMonitorStateHandler ref, err: ProcessError) =>
    None

