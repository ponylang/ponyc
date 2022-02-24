interface ProcessNotify2
  """
  Notifications for Process connections.
  """

  fun ref created(process: ProcessMonitor2 ref) =>
    """
    ProcessMonitor calls this when it is created.
    """

  fun ref failed(process: ProcessMonitor2 ref, err: ProcessError) =>
    """
    ProcessMonitor calls this if we run into errors communicating with the
    forked process.
    """

