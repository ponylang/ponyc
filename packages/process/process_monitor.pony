use "backpressure"
use "collections"
use "files"
use "promises"
use @getpid[I32]() if linux

primitive StartProcess
  """
  Fork+exec (posix) or create (windows) a child process and return a live
  `ProcessMonitor` for it, or a `ProcessError` if the process could not be
  started. Every fallible step of starting the process happens here, before the
  monitor exists, so a returned `ProcessMonitor` is always monitoring a running
  child. Use it in place of a constructor:

  ```pony
  match StartProcess(sp_auth, bp_auth, consume notifier, path, args, vars)
  | let pm: ProcessMonitor => // a live child is running
  | let err: ProcessError  => // never started; err says why
  end
  ```

  On a `ProcessError` return, no process was started and none of the notifier's
  callbacks will fire. On Linux, starting a process requires kernel 5.3 or
  newer; on an older kernel, `StartProcess` returns an `UnsupportedKernel`
  error.
  """
  fun apply(
    auth: StartProcessAuth,
    backpressure_auth: ApplyReleaseBackpressureAuth,
    notifier: ProcessNotify iso,
    path: FilePath,
    args: Array[String] val,
    vars: Array[String] val,
    wdir: (FilePath | None) = None)
  : (ProcessMonitor | ProcessError)
  =>
    // We need permission to execute and the file itself needs to be an
    // executable.
    if not path.caps(FileExec) then
      return ProcessError(CapError, path.path
        + " is not an executable or we do not have execute capability.")
    end

    let is_file = try FileInfo(path)?.file else false end
    if not is_file then
      // Unable to stat the path given, so it may not exist or may be a
      // directory.
      return ProcessError(ExecutableNotFound, path.path
        + " does not exist or is a directory.")
    end

    // On Linux, exit detection uses a pidfd, which needs kernel >= 5.3. Probe
    // on our own pid: ENOSYS means the kernel is too old. Any other probe
    // failure (e.g. out of file descriptors) is left for the child's own
    // pidfd_open to surface as a start failure.
    ifdef linux then
      let probe = @ponyint_pidfd_open(@getpid())
      if probe < 0 then
        if @pony_os_errno() == _ENOSYS() then
          return ProcessError(UnsupportedKernel,
            "the kernel does not support pidfd_open (needs Linux >= 5.3).")
        end
      else
        @close(probe.u32())
      end
    end

    // Create the four pipes as iso so they can cross into the actor. On any
    // failure, close the ones already opened.
    let stdin_pipe: _Pipe iso =
      try recover iso _Pipe.outgoing()? end
      else
        return ProcessError(PipeError, "Failed to open pipe for stdin.")
      end
    let stdout_pipe: _Pipe iso =
      try recover iso _Pipe.incoming()? end
      else
        stdin_pipe.close()
        return ProcessError(PipeError, "Failed to open pipe for stdout.")
      end
    let stderr_pipe: _Pipe iso =
      try recover iso _Pipe.incoming()? end
      else
        stdin_pipe.close()
        stdout_pipe.close()
        return ProcessError(PipeError, "Failed to open pipe for stderr.")
      end
    let err_pipe: _Pipe iso =
      try recover iso _Pipe.incoming()? end
      else
        stdin_pipe.close()
        stdout_pipe.close()
        stderr_pipe.close()
        return ProcessError(PipeError, "Failed to open auxiliary error pipe.")
      end

    // Read the far-end fds off the iso pipes (reading a val field off an iso
    // neither aliases nor consumes it) so the child creation takes plain fds
    // and the pipes stay whole to hand to the monitor.
    let in_far = stdin_pipe.far_fd
    let out_far = stdout_pipe.far_fd
    let err2_far = stderr_pipe.far_fd
    let err_far = err_pipe.far_fd

    // Extract the executable path and working directory as sendable values so
    // they can be used inside the `recover` that builds the child.
    let exec_path: String val = path.path.clone()
    let wdir_str: (String val | None) =
      match \exhaustive\ wdir
      | let d: FilePath => d.path.clone()
      | None => None
      end

    ifdef posix then
      let child: _Process iso =
        try
          recover iso
            _ProcessPosix(exec_path, args, vars, wdir_str, err_far, in_far,
              out_far, err2_far)?
          end
        else
          stdin_pipe.close()
          stdout_pipe.close()
          stderr_pipe.close()
          err_pipe.close()
          return ProcessError(ForkError)
        end
      ProcessMonitor._create(backpressure_auth, consume notifier,
        consume child, consume stdin_pipe, consume stdout_pipe,
        consume stderr_pipe, consume err_pipe)
    elseif windows then
      let windows_child: _ProcessWindows iso =
        recover iso
          _ProcessWindows(exec_path, args, vars, wdir_str, in_far, out_far,
            err2_far)
        end
      // Reading the sendable `process_error` field off the iso neither aliases
      // nor consumes it.
      match windows_child.process_error
      | let pe: ProcessError =>
        stdin_pipe.close()
        stdout_pipe.close()
        stderr_pipe.close()
        err_pipe.close()
        return pe
      end
      ProcessMonitor._create(backpressure_auth, consume notifier,
        consume windows_child, consume stdin_pipe, consume stdout_pipe,
        consume stderr_pipe, consume err_pipe)
    else
      compile_error "unsupported platform"
    end

primitive _Running
primitive _Disposing
primitive _Reaped
type _Lifecycle is (_Running | _Disposing | _Reaped)
  """
  The monitor's lifecycle. A monitor exists only around a running child, so
  there is no "not started" state.

  - `_Running`: child alive, being monitored.
  - `_Disposing`: the client called `dispose()`; the child was killed and the
    pipes closed, and we are awaiting the exit event.
  - `_Reaped`: the child exited and we collected its status (or a reap error);
    absorbing and inert.
  """

actor ProcessMonitor is AsioEventNotify
  """
  Monitors a running child process: forwards its STDOUT/STDERR to a
  `ProcessNotify`, writes to its STDIN, and reports its exit status once it
  exits. Create one with `StartProcess`.

  Exit is detected from a native per-platform event (a pidfd on Linux), not
  from the child's pipes reaching end-of-file, so a child that leaves a
  grandchild holding its stdout/stderr open is still reported as exited
  promptly.
  """
  let _notifier: ProcessNotify
  let _backpressure_auth: ApplyReleaseBackpressureAuth
  let _child: _Process

  var _stdin: _Pipe
  var _stdout: _Pipe
  var _stderr: _Pipe
  var _err: _Pipe
  var _exit_event: AsioEventID = AsioEvent.none()

  let _max_size: USize = 4096
  // Per-pipe bound on the reap-edge drain. A fixed constant of at least one
  // default pipe capacity, so all of a well-behaved child's own output is
  // delivered, while a descendant flooding the pipe the instant the child dies
  // cannot stall teardown. Not a function of the pipe's buffer size, which the
  // child can grow (F_SETPIPE_SZ). This is a security bound.
  let _drain_cap: USize = 1 << 20
  var _read_buf: Array[U8] iso = recover Array[U8] .> undefined(_max_size) end
  var _read_len: USize = 0
  var _expect: USize = 0

  // Backstop for the exit-signal reap retry. Once the OS signals that the child
  // exited, the child is a zombie we own, so waitpid will collect it and the
  // retry converges. The cap only guards a non-convergence that should never
  // occur — a kernel that signals the exit but never lets waitpid reap — where
  // we surrender to WaitpidError rather than spin forever.
  let _reap_retry_cap: U32 = 100_000

  embed _pending: List[(ByteSeq, USize)] = _pending.create()
  var _done_writing: Bool = false
  var _backpressure_applied: Bool = false

  var _state: _Lifecycle = _Running

  new _create(
    backpressure_auth: ApplyReleaseBackpressureAuth,
    notifier: ProcessNotify iso,
    child: _Process iso,
    stdin: _Pipe iso,
    stdout: _Pipe iso,
    stderr: _Pipe iso,
    err: _Pipe iso)
  =>
    """
    Build a monitor around an already-running child and its pipes. Called by
    `StartProcess` (and by the in-package test factory). Not public: a monitor
    is only ever created around a live child.
    """
    _backpressure_auth = backpressure_auth
    _notifier = consume notifier
    _child = consume child
    _stdin = consume stdin
    _stdout = consume stdout
    _stderr = consume stderr
    _err = consume err

    // Register each pipe's asio event and close its far end.
    _stdin.begin(this)
    _stdout.begin(this)
    _stderr.begin(this)
    _err.begin(this)

    // Arm the native exit event.
    _exit_event = _child.arm_exit_event(this)

    _notifier.created(this)

    // The child may have exited before we armed the exit event; under
    // edge-triggered notification that readable edge can be missed, so probe
    // once now. A duplicate real event later lands in `_Reaped` and is a no-op.
    _reap_if_exited()

  be print(data: ByteSeq) =>
    """
    Print some bytes and append a newline.
    """
    if (_state is _Running) and (not _done_writing) then
      _write_final(data)
      _write_final("\n")
    end

  be write(data: ByteSeq) =>
    """
    Write to STDIN of the child process.
    """
    if (_state is _Running) and (not _done_writing) then
      _write_final(data)
    end

  be printv(data: ByteSeqIter) =>
    """
    Print an iterable collection of ByteSeqs.
    """
    if _state is _Running then
      for bytes in data.values() do
        _write_final(bytes)
        _write_final("\n")
      end
    end

  be writev(data: ByteSeqIter) =>
    """
    Write an iterable collection of ByteSeqs.
    """
    if _state is _Running then
      for bytes in data.values() do
        _write_final(bytes)
      end
    end

  be done_writing() =>
    """
    Signal that we are finished writing to STDIN. Once any pending writes have
    drained, STDIN is closed so a child waiting on EOF can proceed.
    """
    if _state is _Running then
      _done_writing = true
      if _pending.size() == 0 then
        _close_stdin()
      end
    end

  be dispose() =>
    """
    Terminate the child and stop monitoring. The exit status of the killed
    child is reported through `ProcessNotify.dispose` once its exit event
    fires.
    """
    match \exhaustive\ _state
    | _Running =>
      _state = _Disposing
      _child.kill()
      // Close the pipes (releasing any backpressure). The exit event stays
      // armed so the reap still fires and reports the status.
      _close_stdin()
      _stdout.close()
      _stderr.close()
      _err.close()
    | _Disposing => None // already disposing; do not kill again
    | _Reaped => None    // already reaped; never kill a reaped pid
    end

  fun ref expect(qty: USize = 0) =>
    """
    A `stdout` call on the notifier must contain exactly `qty` bytes. If
    `qty` is zero, the call can contain any amount of data.
    """
    _expect = _notifier.expect(this, qty)
    _read_buf_size()

  be _event_notify(event: AsioEventID, flags: U32, arg: U32) =>
    """
    Handle an asio event from one of the pipes or from the exit source.
    """
    if (_exit_event isnt AsioEvent.none()) and (event is _exit_event) then
      if AsioEvent.disposable(flags) then
        @pony_asio_event_destroy(_exit_event)
        _exit_event = AsioEvent.none()
      elseif AsioEvent.errored(flags) then
        _on_exit_event_error()
      else
        _reap_on_exit_signal(0)
      end
      return
    end

    match event
    | _stdin.event =>
      if AsioEvent.writeable(flags) then
        _pending_writes()
      elseif AsioEvent.errored(flags) then
        _close_stdin()
      elseif AsioEvent.disposable(flags) then
        _stdin.dispose()
      end
    | _stdout.event =>
      if AsioEvent.readable(flags) then
        _read_pipe(_stdout)
      elseif AsioEvent.errored(flags) then
        _stdout.close()
      elseif AsioEvent.disposable(flags) then
        _stdout.dispose()
      end
    | _stderr.event =>
      if AsioEvent.readable(flags) then
        _read_pipe(_stderr)
      elseif AsioEvent.errored(flags) then
        _stderr.close()
      elseif AsioEvent.disposable(flags) then
        _stderr.dispose()
      end
    | _err.event =>
      if AsioEvent.readable(flags) then
        _read_pipe(_err)
      elseif AsioEvent.errored(flags) then
        _err.close()
      elseif AsioEvent.disposable(flags) then
        _err.dispose()
      end
    end

  fun ref _reap_if_exited() =>
    """
    Proactive start-up reap: the child may have exited before the exit event was
    armed. Gated on state so it runs at most once. If the child has not exited,
    do nothing — the exit signal will arrive and drive the reap through
    `_reap_on_exit_signal`.
    """
    if _state is _Reaped then
      return
    end

    match \exhaustive\ _child.wait()
    | let status: ProcessExitStatus =>
      _do_reap(status)
    | WaitpidError =>
      _do_reap_error()
    | _StillRunning =>
      None // child still running; the exit signal will drive the reap
    end

  fun ref _reap_on_exit_signal(attempt: U32) =>
    """
    Reap after the OS signalled that the child exited (a real exit event, or the
    ESRCH-synthesized one on kqueue). Gated on state so it runs at most once.
    `waitpid` can briefly still report the child as running, because the OS exit
    notification arrives ahead of waitpid; on `_StillRunning` retry through a
    self-message, so the reap never blocks a scheduler thread, bounded by
    `_reap_retry_cap`.
    """
    if _state is _Reaped then
      return
    end

    match \exhaustive\ _child.wait()
    | let status: ProcessExitStatus =>
      _do_reap(status)
    | WaitpidError =>
      _do_reap_error()
    | _StillRunning =>
      if attempt < _reap_retry_cap then
        _reap_again(attempt + 1)
      else
        _do_reap_error()
      end
    end

  be _reap_again(attempt: U32) =>
    """
    One yielding hop of the exit-signal reap retry (see `_reap_on_exit_signal`).
    """
    _reap_on_exit_signal(attempt)

  fun ref _do_reap(status: ProcessExitStatus) =>
    """
    The reap edge. One behavior turn, no yield. On a natural exit (`_Running`),
    drain the child's last output before reporting; on a disposed exit
    (`_Disposing`), the client cancelled, so skip the drain.
    """
    let was_running = _state is _Running
    _state = _Reaped
    if was_running then
      _drain()
    end
    _close_all()
    _notifier.dispose(this, status)

  fun ref _do_reap_error() =>
    """
    The reap returned an error rather than a status. Report `failed`; there is
    no exit status to `dispose` with.
    """
    _state = _Reaped
    _close_all()
    _notifier.failed(this, ProcessError(WaitpidError))

  fun ref _on_exit_event_error() =>
    """
    The exit event's subscription errored, so we have lost exit detection. Try
    a last reap; otherwise report failure and stop, without killing the child —
    a monitor-side fault must not change the child's fate.
    """
    if _state is _Reaped then
      return
    end

    match _child.wait()
    | let status: ProcessExitStatus =>
      _do_reap(status)
    else
      _state = _Reaped
      _close_all()
      _notifier.failed(this, ProcessError(WaitpidError))
    end

  fun ref _drain() =>
    """
    Read whatever the child left in its pipes before it exited, so its last
    output is delivered before `dispose`. Draining `_err` is required: an
    exec/chdir failure writes one byte there, and we must report it before
    closing the pipe. Bounded per pipe by `_drain_cap`.
    """
    _read_from(_stdout, true)
    _read_from(_stderr, true)
    _read_from(_err, true)

  fun ref _close_all() =>
    """
    Close all four pipes (both ends) and the exit source. Idempotent.
    """
    _close_stdin()
    _stdout.close()
    _stderr.close()
    _err.close()
    _close_exit_source()

  fun ref _close_stdin() =>
    """
    Close STDIN, release any applied backpressure, and drop pending writes.
    Every running-state stdin close routes through here so backpressure can
    never be stranded.
    """
    _stdin.close()
    _release_backpressure()
    _pending.clear()

  fun ref _close_exit_source() =>
    if _exit_event isnt AsioEvent.none() then
      _child.close_exit_source(_exit_event)
      // Leave `_exit_event` set: it is destroyed on its `disposable` callback.
    end

  fun ref _apply_backpressure() =>
    if not _backpressure_applied then
      _backpressure_applied = true
      Backpressure.apply(_backpressure_auth)
    end

  fun ref _release_backpressure() =>
    if _backpressure_applied then
      _backpressure_applied = false
      Backpressure.release(_backpressure_auth)
    end

  fun ref _read_pipe(pipe: _Pipe) =>
    _read_from(pipe, false)

  fun ref _read_from(pipe: _Pipe, draining: Bool) =>
    """
    Read from a pipe while data is available. Running (`draining = false`):
    yield after 4 kb via a self-message to avoid starving other actors.
    Draining (`draining = true`): no yield, capped at `_drain_cap`, for the
    reap edge which must stay one atomic behavior turn.

    It is safe to use the same buffer for stdout, stderr, and err because of
    causal messaging: events are processed one after another.
    """
    if pipe.is_closed() then return end
    var sum: USize = 0
    while true do
      (_read_buf, let len, let errno) =
        pipe.read(_read_buf = recover Array[U8] end, _read_len)

      let next = _read_buf.space()
      match len
      | -1 =>
        if errno == _EAGAIN() then
          return // nothing to read right now, try again later
        end
        pipe.close()
        return
      | 0 =>
        pipe.close()
        return
      end

      _read_len = _read_len + len.usize()

      let data = _read_buf = recover Array[U8] .> undefined(next) end
      data.truncate(_read_len)

      match pipe.near_fd
      | _stdout.near_fd =>
        if _read_len >= _expect then
          _notifier.stdout(this, consume data)
        end
      | _stderr.near_fd =>
        _notifier.stderr(this, consume data)
      | _err.near_fd =>
        let step: U8 = try data.read_u8(0)? else -1 end
        match step
        | _StepChdir() =>
          _notifier.failed(this, ProcessError(ChdirError))
        | _StepExecve() =>
          _notifier.failed(this, ProcessError(ExecveError))
        else
          _notifier.failed(this, ProcessError(UnknownError))
        end
      end

      _read_len = 0
      _read_buf_size()

      sum = sum + len.usize()
      if draining then
        if sum >= _drain_cap then
          return
        end
      else
        if sum > (1 << 12) then
          // If we've read 4 kb, yield and read again later.
          _read_again(pipe.near_fd)
          return
        end
      end
    end

  fun ref _read_buf_size() =>
    if _expect > 0 then
      _read_buf.undefined(_expect)
    else
      _read_buf.undefined(_max_size)
    end

  be _read_again(near_fd: U32) =>
    """
    Resume reading on a file descriptor after a yield.
    """
    if _state is _Running then
      match near_fd
      | _stdout.near_fd => _read_pipe(_stdout)
      | _stderr.near_fd => _read_pipe(_stderr)
      | _err.near_fd => _read_pipe(_err)
      end
    end

  fun ref _write_final(data: ByteSeq) =>
    """
    Write as much as possible to STDIN if it is open and there are no pending
    writes. Queue everything unwritten and apply backpressure. If STDIN is
    already closed, drop the write rather than queue it — nothing would drain
    the queue or release the backpressure.
    """
    if _stdin.is_closed() then
      return
    end

    if _pending.size() == 0 then
      // Send as much data as possible.
      (let len, let errno) = _stdin.write(data, 0)

      if len == -1 then // write error
        if errno == _EAGAIN() then
          // Resource temporarily unavailable, send data later.
          _pending.push((data, 0))
          _apply_backpressure()
        else
          // Notify caller of error, close fd and done.
          _notifier.failed(this, ProcessError(WriteError))
          _close_stdin()
        end
      elseif len.usize() < data.size() then
        // Send any remaining data later.
        _pending.push((data, len.usize()))
        _apply_backpressure()
      end
    else
      // Send later, when the pipe is available for writing.
      _pending.push((data, 0))
      _apply_backpressure()
    end

  fun ref _pending_writes() =>
    """
    Send any pending data. If any data can't be sent, keep it in _pending.
    Once _pending is non-empty, direct writes get queued there, and they can
    only be written here. If _done_writing is set, close STDIN once the queue
    drains.
    """
    while (not _stdin.is_closed()) and (_pending.size() > 0) do
      try
        let node = _pending.head()?
        (let data, let offset) = node()?

        // Write as much data as possible.
        (let len, let errno) = _stdin.write(data, offset)

        if len == -1 then // OS signals write error
          if errno == _EAGAIN() then
            // Resource temporarily unavailable, send data later.
            return
          else
            // Close pipe and bail out.
            _notifier.failed(this, ProcessError(WriteError))
            _close_stdin()
            return
          end
        elseif (len.usize() + offset) < data.size() then
          // Send remaining data later.
          node()? = (data, offset + len.usize())
          return
        else
          // This pending chunk has been fully sent.
          _pending.shift()?
          if _pending.size() == 0 then
            _release_backpressure()
            // Close STDIN if the client is done writing.
            if _done_writing then
              _close_stdin()
            end
          end
        end
      else
        // handle error
        _notifier.failed(this, ProcessError(WriteError))
        return
      end
    end

  // --- test seam (package-private) ---------------------------------------
  // These exist so a package-private test factory can drive the state machine
  // with a spy `_Process` and placeholder pipes. They have no public surface.

  be _test_trigger_exit() =>
    """
    Simulate the native exit event firing. The spy `_Process.wait()` returns
    the chosen status, driving the reap through the same gated path the real
    event uses.
    """
    _reap_on_exit_signal(0)

  be _test_query_backpressure(p: Promise[Bool]) =>
    """
    Report whether backpressure is currently applied, so a test can assert it
    was released after a terminal edge.
    """
    p(_backpressure_applied)
