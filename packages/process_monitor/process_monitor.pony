use "backpressure"
use "collections"
use "files"
use "time"

primitive ProcessMonitorCreator
  """
  Fork+execs / creates a child process and returns an actor that monitors the
  process.
  """
  fun apply(
    auth: StartProcessAuth,
    notifier: ProcessNotify iso,
    executable: FilePath,
    args: Array[String] val,
    vars: Array[String] val,
    wdir: (FilePath | None) = None)
  : ProcessMonitor ?
  =>
    // TODO: improve on the "just an error" results
    FileInfo(executable)?.file

    if not executable.caps(FileExec) then
      error
    end

    // TODO: handle shutting things down if any fail
    let stdin: _Pipe iso = recover iso _Pipe.outgoing()? end
    let stdout: _Pipe iso = recover iso _Pipe.incoming()? end
    let stderr: _Pipe iso = recover iso _Pipe.incoming()? end
    let err: _Pipe iso = recover iso _Pipe.incoming()? end

    ifdef posix then
      let child: _Process iso = _ProcessPosix.create(
        executable.path, args, vars, wdir, err, stdin, stdout, stderr)?

        return ProcessMonitor(consume child,
          consume notifier,
          consume stdin,
          consume stdout,
          consume stderr,
          consume err)
    elseif windows then
      let child: _Process iso = _ProcessWindows.create(
        executable.path, args, vars, wdir, stdin, stdout, stderr)
      // notify about errors
      match child.process_error
      | let pe: ProcessError =>
        error
      end

      return ProcessMonitor(consume child,
        consume notifier,
        consume stdin,
        consume stdout,
        consume stderr,
        consume err)
    else
      compile_error "unsupported platform"
    end

actor ProcessMonitor is AsioEventNotify
  let _process: _Process
  let _notifier: ProcessNotify
  let _stdin: _Pipe
  let _stdout: _Pipe
  let _stderr: _Pipe
  let _err: _Pipe

  // For windows due to lack of ASIO.
  // We used this for polling.
  // TODO: We could
  // consider that perhaps life would be easier
  // with Posix if we didn't use ASIO and only polled
  // Realistically, do we need ASIO for this?
  var _timers: (Timers tag | None) = None

  new _create(
    process: _Process iso,
    notifier: ProcessNotify iso,
    stdin: _Pipe iso,
    stdout: _Pipe iso,
    stderr: _Pipe iso,
    err: _Pipe iso)
  =>
    _process = consume process
    _notifier = consume notifier
    _stdin = consume stdin
    _stdout = consume stdout
    _stderr = consume stderr
    _err = consume err

    _stdin.begin(this)
    _stdout.begin(this)
    _stderr.begin(this)
    _err.begin(this)

    // Asio is not wired up for Windows, so use a timer for now.
    ifdef windows then
      _setup_windows_timers()
    end
    _notifier.created(this)

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

  fun ref _ensure_timers(): Timers =>
    match _timers
    | None =>
      let ts = Timers
      _timers = ts
      ts
    | let ts: Timers => ts
    end

  be _event_notify(event: AsioEventID, flags: U32, arg: U32) =>
    None
