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
      let child: _ProcessPosix iso = recover iso _ProcessPosix(
        executable.path,
        args,
        vars,
        wdir,
        consume err,
        consume stdin,
        consume stdout,
        consume stderr)?
      end

      return _PosixProcessMonitor(consume child, consume notifier)
    elseif windows then
      let child: _ProcessWindows iso = recover iso _ProcessWindows(
        executable.path,
        args,
        vars,
        wdir,
        consume stdin,
        consume stdout,
        consume stderr)
      end

      // notify about errors
      match child.process_error
      | let pe: ProcessError =>
        error
      end

      return _WindowsProcessMonitor(consume child, consume notifier)
    else
      compile_error "unsupported platform"
    end

interface tag ProcessMonitor

actor _PosixProcessMonitor is AsioEventNotify
  let _process: _ProcessPosix
  let _notifier: ProcessNotify

  new create(process: _ProcessPosix iso, notifier: ProcessNotify iso) =>
    _process = consume process
    _notifier = consume notifier

    _process.link(this)
    _notifier.created(this)

  be _event_notify(event: AsioEventID, flags: U32, arg: U32) =>
    // TODO: implementation
    None

actor _WindowsProcessMonitor
  let _process: _ProcessWindows
  let _notifier: ProcessNotify
  let _timers: Timers = Timers

  new create(process: _ProcessWindows iso, notifier: ProcessNotify iso) =>
    _process = consume process
    _notifier = consume notifier

    _setup_timers()
    _notifier.created(this)

  fun ref _setup_timers() =>
    let pm: _WindowsProcessMonitor tag = this
    let tn =
      object iso is TimerNotify
        fun ref apply(timer: Timer, count: U64): Bool =>
          pm.timer_notify()
          true
      end
    let timer = Timer(consume tn, 50_000_000, 10_000_000)
    _timers(consume timer)

  be timer_notify() =>
    """
    Windows IO polling timer has fired
    """

    // TODO: implementation
    None
