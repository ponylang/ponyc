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
    Called for an error while the child runs — a failed write to its STDIN, or
    an exec/chdir failure the child reported as it started. These are followed
    by `dispose` once the child exits. `failed` is also called once, with a
    `WaitpidError`, if the child's exit status cannot be read; that is terminal
    and replaces `dispose`.
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
    Called with the child's exit status once the child has exited. A monitor
    only exists around a running child, so `created` is always called first.
    While the child runs, operational errors may arrive through `failed`; then,
    when the child exits, `dispose` is called with its status. If the exit
    status itself cannot be read, a terminal `failed` is called in place of
    `dispose`. `dispose` is called at most once.

    `dispose` includes the exit status of the child process. If the process
    finished, then `child_exit_status` will be an instance of [Exited](process-Exited.md).

    The child's exit code can be retrieved from the `Exited` instance by using
    [Exited.exit_code()](process-Exited.md#exit_code).

    On Posix systems, if the process has been killed by a signal (e.g. through
    the `kill` command), `child_exit_status` will be an instance of
    [Signaled](process-Signaled.md) with the signal number that terminated the
    process available via [Signaled.signal()](process-Signaled.md#signal).
    """
