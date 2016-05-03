interface ProcessNotify
  """
  Notifications for Process connections.
  """
    
  fun ref stdout(data: Array[U8] iso) =>
    """
    ProcessMonitor calls this when new data is received on STDOUT of the 
    forked process
    """
    
  fun ref stderr(data: Array[U8] iso) =>
    """
    ProcessMonitor calls this when new data is received on STDERR of the 
    forked process
    """

  fun ref failed(err: ProcessError) =>
    """
    ProcessMonitor calls this if we run into errors communicating with the
    forked process.
    """

  fun ref dispose(child_exit_code: I32) =>
    """
    Call when ProcessMonitor terminates to cleanup ProcessNotify.
    We return the exit code of the child process.
    """
