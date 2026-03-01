use "backpressure"
use "collections"
use "files"
use "time"

actor ProcessMonitor is AsioEventNotify
  """
  Fork+execs / creates a child process and monitors it. Notifies a client about
  STDOUT / STDERR events.
  """
  let _notifier: ProcessNotify
  let _backpressure_auth: ApplyReleaseBackpressureAuth

  var _stdin: _Pipe = _Pipe.none()
  var _stdout: _Pipe = _Pipe.none()
  var _stderr: _Pipe = _Pipe.none()
  var _err: _Pipe = _Pipe.none()
  var _child: _Process = _ProcessNone

  let _max_size: USize = 4096
  var _read_buf: Array[U8] iso = recover Array[U8] .> undefined(_max_size) end
  var _read_len: USize = 0
  var _expect: USize = 0

  embed _pending: List[(ByteSeq, USize)] = _pending.create()
  var _done_writing: Bool = false

  var _closed: Bool = false
  var _timers: (Timers tag | None) = None
  let _process_poll_interval: U64

  var _polling_child: Bool = false
  var _final_wait_result: (_WaitResult | None) = None

  new create(
    auth: StartProcessAuth,
    backpressure_auth: ApplyReleaseBackpressureAuth,
    notifier: ProcessNotify iso,
    filepath: FilePath,
    args: Array[String] val,
    vars: Array[String] val,
    wdir: (FilePath | None) = None,
    process_poll_interval: U64 = Nanos.from_millis(100))
  =>
    """
    Create infrastructure to communicate with a forked child process and
    register the asio events. Fork child process and notify our user about
    incoming data via the notifier.
    """
    _backpressure_auth = backpressure_auth
    _notifier = consume notifier
    _process_poll_interval = process_poll_interval

    // We need permission to execute and the
    // file itself needs to be an executable
    if not filepath.caps(FileExec) then
      _notifier.failed(this, ProcessError(CapError, filepath.path
        + " is not an executable or we do not have execute capability."))
      return
    end

    let ok = try
      FileInfo(filepath)?.file
    else
      false
    end
    if not ok then
      // unable to stat the file path given so it may not exist
      // or may be a directory.
      _notifier.failed(this, ProcessError(ExecveError, filepath.path
        + " does not exist or is a directory."))
      return
    end

    try
      _stdin = _Pipe.outgoing()?
    else
      _stdin.close()
      _notifier.failed(this, ProcessError(PipeError,
        "Failed to open pipe for stdin."))
      return
    end

    try
      _stdout = _Pipe.incoming()?
    else
      _stdin.close()
      _stdout.close()
      _notifier.failed(this, ProcessError(PipeError,
        "Failed to open pipe for stdout."))
      return
    end

    try
      _stderr = _Pipe.incoming()?
    else
      _stdin.close()
      _stdout.close()
      _stderr.close()
      _notifier.failed(this, ProcessError(PipeError,
        "Failed to open pipe for stderr."))
      return
    end

    try
      _err = _Pipe.incoming()?
    else
      _stdin.close()
      _stdout.close()
      _stderr.close()
      _err.close()
      _notifier.failed(this, ProcessError(PipeError,
        "Failed to open auxiliary error pipe."))
      return
    end

    try
      ifdef posix then
        _child = _ProcessPosix.create(
          filepath.path, args, vars, wdir, _err, _stdin, _stdout, _stderr)?
      elseif windows then
        let windows_child = _ProcessWindows.create(
          filepath.path, args, vars, wdir, _stdin, _stdout, _stderr)
        _child = windows_child
        // notify about errors
        match windows_child.process_error
        | let pe: ProcessError =>
          _notifier.failed(this, pe)
          return
        end
      else
        compile_error "unsupported platform"
      end
      _err.begin(this)
      _stdin.begin(this)
      _stdout.begin(this)
      _stderr.begin(this)
    else
      _notifier.failed(this, ProcessError(ForkError))
      return
    end

    // Asio is not wired up for Windows, so use a timer for now.
    ifdef windows then
      _setup_windows_timers()
    end
    _notifier.created(this)


  be print(data: ByteSeq) =>
    """
    Print some bytes and append a newline.
    """
    if not _done_writing then
      _write_final(data)
      _write_final("\n")
    end

  be write(data: ByteSeq) =>
    """
    Write to STDIN of the child process.
    """
    if not _done_writing then
      _write_final(data)
    end

  be printv(data: ByteSeqIter) =>
    """
    Print an iterable collection of ByteSeqs.
    """
    for bytes in data.values() do
      _write_final(bytes)
      _write_final("\n")
    end

  be writev(data: ByteSeqIter) =>
    """
    Write an iterable collection of ByteSeqs.
    """
    for bytes in data.values() do
      _write_final(bytes)
    end

  be done_writing() =>
    """
    Set the _done_writing flag to true. If _pending is empty we can close the
    _stdin pipe.
    """
    _done_writing = true
    Backpressure.release(_backpressure_auth)
    if _pending.size() == 0 then
      _stdin.close_near()
    end

  be dispose() =>
    """
    Terminate child and close down everything.
    """
    match _child
    | let never_started: _ProcessNone =>
      // We never started a child process so do not do any disposal
      // If we do, some weirdness can happen with dispose getting called
      // on the notifier which is not supposed to happen if we never started
      // a child (or haven't started one yet).
      return
    else
      Backpressure.release(_backpressure_auth)
      _child.kill()
      _close()
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
    Handle the incoming Asio event from one of the pipes.
    """
    match event
    | _stdin.event =>
      if AsioEvent.writeable(flags) then
        _pending_writes()
      elseif AsioEvent.disposable(flags) then
        _stdin.dispose()
      end
    | _stdout.event =>
      if AsioEvent.readable(flags) then
        _pending_reads(_stdout)
      elseif AsioEvent.disposable(flags) then
        _stdout.dispose()
      end
    | _stderr.event =>
      if AsioEvent.readable(flags) then
        _pending_reads(_stderr)
      elseif AsioEvent.disposable(flags) then
        _stderr.dispose()
      end
    | _err.event =>
      if AsioEvent.readable(flags) then
        _pending_reads(_err)
      elseif AsioEvent.disposable(flags) then
        _err.dispose()
      end
    end
    _try_shutdown()

  be timer_notify() =>
    """
    Windows IO polling timer has fired
    """
    _pending_writes() // try writes
    _pending_reads(_stdout)
    _pending_reads(_stderr)
    _try_shutdown()

  fun ref _close() =>
    """
    Close all pipes and wait for the child process to exit.
    """
    if not _closed then
      _closed = true
      _stdin.close()
      _stdout.close()
      _stderr.close()
      _wait_for_child()
    end

  be _wait_for_child() =>
    match _final_wait_result
    | let wr: _WaitResult =>
      None
    else
      match \exhaustive\ _child.wait()
      | let sr: _StillRunning =>
        if not _polling_child then
          _polling_child = true
          let timers = _ensure_timers()
          let pm: ProcessMonitor tag = this
          let tn =
            object iso is TimerNotify
              fun ref apply(timer: Timer, count: U64): Bool =>
                pm._wait_for_child()
                true
            end
          let timer = Timer(consume tn, _process_poll_interval, _process_poll_interval)
          timers(consume timer)
        end
      | let exit_status: ProcessExitStatus =>
        // process child exit code or termination signal
        _final_wait_result = exit_status
        _notifier.dispose(this, exit_status)
        _dispose_timers()
      | let wpe: WaitpidError =>
        _final_wait_result = wpe
        _notifier.failed(this, ProcessError(WaitpidError))
        _dispose_timers()
      end
    end

  fun ref _ensure_timers(): Timers tag =>
    match \exhaustive\ _timers
    | None =>
      let ts = Timers
      _timers = ts
      ts
    | let ts: Timers => ts
    end

  fun ref _dispose_timers() =>
    match _timers
    | let ts: Timers =>
      ts.dispose()
      _timers = None
    end

  fun ref _setup_windows_timers() =>
    let timers = _ensure_timers()
    let pm: ProcessMonitor tag = this
    let tn =
      object iso is TimerNotify
        fun ref apply(timer: Timer, count: U64): Bool =>
          pm.timer_notify()
          true
      end
    let timer = Timer(consume tn, 50_000_000, 10_000_000)
    timers(consume timer)

  fun ref _try_shutdown() =>
    """
    If neither stdout nor stderr are open we close down and exit.
    """
    if _stdin.is_closed() and
      _stdout.is_closed() and
      _stderr.is_closed()
    then
       _close()
    end

  fun ref _pending_reads(pipe: _Pipe) =>
    """
    Read from stdout or stderr while data is available. If we read 4 kb of
    data, send ourself a resume message and stop reading, to avoid starving
    other actors.
    It's safe to use the same buffer for stdout and stderr because of
    causal messaging. Events get processed one _after_ another.
    """
    if pipe.is_closed() then return end
    var sum: USize = 0
    while true do
      (_read_buf, let len, let errno) =
        pipe.read(_read_buf = recover Array[U8] end, _read_len)

      let next = _read_buf.space()
      match len
      | -1 =>
        if (errno == _EAGAIN()) then
          return // nothing to read right now, try again later
        end
        pipe.close()
        return
      | 0  =>
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
      if sum > (1 << 12) then
        // If we've read 4 kb, yield and read again later.
        _read_again(pipe.near_fd)
        return
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
    Resume reading on file descriptor.
    """
    match near_fd
    | _stdout.near_fd => _pending_reads(_stdout)
    | _stderr.near_fd => _pending_reads(_stderr)
    end

  fun ref _write_final(data: ByteSeq) =>
    """
    Write as much as possible to the pipe if it is open and there are no
    pending writes. Save everything unwritten into _pending and apply
    backpressure.
    """
    if (not _closed) and not _stdin.is_closed() and (_pending.size() == 0) then
      // Send as much data as possible.
      (let len, let errno) = _stdin.write(data, 0)

      if len == -1 then // write error
        if errno == _EAGAIN() then
          // Resource temporarily unavailable, send data later.
          _pending.push((data, 0))
          Backpressure.apply(_backpressure_auth)
        else
          // Notify caller of error, close fd and done.
          _notifier.failed(this, ProcessError(WriteError))
          _stdin.close_near()
        end
      elseif len.usize() < data.size() then
        // Send any remaining data later.
        _pending.push((data, len.usize()))
        Backpressure.apply(_backpressure_auth)
      end
    else
      // Send later, when the pipe is available for writing.
      _pending.push((data, 0))
      Backpressure.apply(_backpressure_auth)
    end

  fun ref _pending_writes() =>
    """
    Send any pending data. If any data can't be sent, keep it in _pending.
    Once _pending is non-empty, direct writes will get queued there,
    and they can only be written here. If the _done_writing flag is set, close
    the pipe once we've processed pending writes.
    """
    while (not _closed) and not _stdin.is_closed() and (_pending.size() > 0) do
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
            _stdin.close_near()
            return
          end
        elseif (len.usize() + offset) < data.size() then
          // Send remaining data later.
          node()? = (data, offset + len.usize())
          return
        else
          // This pending chunk has been fully sent.
          _pending.shift()?
          if (_pending.size() == 0) then
            Backpressure.release(_backpressure_auth)
            // check if the client has signaled it is done
            if _done_writing then
              _stdin.close_near()
            end
          end
        end
      else
        // handle error
        _notifier.failed(this, ProcessError(WriteError))
        return
      end
    end
