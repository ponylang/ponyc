interface ProcessNotify
  """
  Notifications for Process connections.
  """

  fun ref created(process: ProcessMonitor ref) =>
    """
    ProcessMonitor calls this when it is created.
    """

  fun ref stdout(process: ProcessMonitor ref, data: Array[U8] iso) =>
    """
    ProcessMonitor calls this when new data is received on STDOUT of the
    forked process
    """

  fun ref stderr(process: ProcessMonitor ref, data: Array[U8] iso) =>
    """
    ProcessMonitor calls this when new data is received on STDERR of the
    forked process
    """

  fun ref failed(process: ProcessMonitor ref, err: ProcessError) =>
    """
    ProcessMonitor calls this if we run into errors communicating with the
    forked process.
    """

  fun ref expect(process: ProcessMonitor ref, qty: USize): USize =>
    """
    Called when the process monitor has been told to expect a certain quantity
    of bytes. This allows nested notifiers to change the expected quantity,
    which allows a lower level protocol to handle any framing.
    """
    qty

  fun ref dispose(process: ProcessMonitor ref, child_exit_code: I32) =>
    """
    Call when ProcessMonitor terminates to cleanup ProcessNotify.
    We return the exit code of the child process.
    """
