use "files"
use "time"

actor ProcessMonitor2 is AsioEventNotify
  """
  Fork+execs / creates a child process and monitors it. Notifies a client about
  STDOUT / STDERR events.
  """
  var _state_handler: _ProcessMonitorStateHandler = _ProcessNotStarted
  let _notifier: ProcessNotify2

  var _stdin: _Pipe = _Pipe.none()
  var _stdout: _Pipe = _Pipe.none()
  var _stderr: _Pipe = _Pipe.none()
  var _err: _Pipe = _Pipe.none()

  // TODO: should be (_Process | None)
  var _child: _Process = _ProcessNone

  var _timers: (Timers tag | None) = None

  new create(
    auth: ProcessMonitorAuth,
    notifier: ProcessNotify2 iso,
    filepath: FilePath,
    args: Array[String] val,
    vars: Array[String] val,
    wdir: (FilePath | None) = None)
  =>
    _notifier = consume notifier

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

    ifdef posix then
      try
        _child = _ProcessPosix.create(
          filepath.path, args, vars, wdir, _err, _stdin, _stdout, _stderr)?
      else
        _process_failed_to_start(ProcessError(ForkError))
      end
    elseif windows then
      let windows_child = _ProcessWindows.create(
        filepath.path, args, vars, wdir, _stdin, _stdout, _stderr)
      _child = windows_child
      match windows_child.process_error
      | let pe: ProcessError =>
        _process_failed_to_start(pe)
      else
        _finish_starting()
      end
    else
      compile_error "unsupported platform"
    end

  fun ref _process_failed_to_start(pe: ProcessError) =>
    _state_handler = _ProcessFailed
    _notifier.failed(this, pe)

  fun ref _finish_starting() =>
    _state_handler = _ProcessRunning
    _err.begin(this)
    _stdin.begin(this)
    _stdout.begin(this)
    _stderr.begin(this)

    // Asio is not wired up for Windows, so use a timer for now.
    ifdef windows then
      _setup_windows_timers()
    end
    _notifier.created(this)

  fun ref _setup_windows_timers() =>
    let timers = _ensure_timers()
    let pm: ProcessMonitor2 tag = this
    let tn =
      object iso is TimerNotify
        fun ref apply(timer: Timer, count: U64): Bool =>
          pm.timer_notify()
          true
      end
    let timer = Timer(consume tn, 50_000_000, 10_000_000)
    timers(consume timer)

  fun ref _ensure_timers(): Timers tag =>
    match _timers
    | None =>
      let ts = Timers
      _timers = ts
      ts
    | let ts: Timers => ts
    end

  be timer_notify() =>
    """
    Windows IO polling timer has fired
    """
    // TODO: sean
    None

  be _event_notify(event: AsioEventID, flags: U32, arg: U32) =>
    """
    Handle the incoming Asio event from one of the pipes.
    """
    // TODO: sean
    None
