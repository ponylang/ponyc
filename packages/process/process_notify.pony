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

  fun ref dispose(process: ProcessMonitor ref, child_exit_status: ProcessExitStatus) =>
    """
    Called when ProcessMonitor terminates to cleanup ProcessNotify if a child
    process was in fact started. `dispose` will not be called if a child process
    was never started. The notify will know that a process was started and to
    expect a `dispose` call by having received a `created` call.

    `dispose` includes the exit status of the child process. If the process
    finished, then `child_exit_status` will be an instance of [Exited](process-Exited.md).

    The childs exit code can be retrieved from the `Exited` instance by using
    [Exited.exit_code()](process-Exited.md#exit_code).

    On Posix systems, if the process has been killed by a signal (e.g. through
    the `kill` command), `child_exit_status` will be an instance of
    [Signaled](process-Signaled.md) with the signal number that terminated the
    process available via [Signaled.signal()](process-Signaled.md#signal).
    """
