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

      // TODO: we really need to notice a posix startup failure here.
      // in the existing, it is done in _pending_reads with _err.near_fd
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

trait tag ProcessMonitor
  be dispose()
  be done_writing()
  be print(data: ByteSeq)
  be write(data: ByteSeq)
  be printv(data: ByteSeqIter)
  be writev(data: ByteSeqIter)

actor _PosixProcessMonitor is (ProcessMonitor & AsioEventNotify)
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

  be dispose() =>
    // TODO: implementation
    None

  be done_writing() =>
    // TODO: implementation
    None

  be print(data: ByteSeq) =>
    // TODO: implementation
    None

  be write(data: ByteSeq) =>
    // TODO: implementation
    None

  be printv(data: ByteSeqIter) =>
    // TODO: implementation
    None

  be writev(data: ByteSeqIter) =>
    // TODO: implementation
    None

actor _WindowsProcessMonitor is ProcessMonitor
  let _process: _ProcessWindows
  let _notifier: ProcessNotify
  let _timers: Timers = Timers

  new create(process: _ProcessWindows iso, notifier: ProcessNotify iso) =>
    _process = consume process
    _notifier = consume notifier

    _setup_timers()
    // TODO: we need to do some kind of calling of "begin" on our process
    // pipes to close the far end but currently that requires an AsioEventNotify
    _notifier.created(this)

  fun ref _setup_timers() =>
    let pm: _WindowsProcessMonitor tag = this
    let tn =
      object iso is TimerNotify
        fun ref apply(timer: Timer, count: U64): Bool =>
          pm._timer_notify()
          true
      end
    let timer = Timer(consume tn, 50_000_000, 10_000_000)
    _timers(consume timer)

  be _timer_notify() =>
    """
    Windows IO polling timer has fired
    """

    // TODO: implementation
    None

  be dispose() =>
    // TODO: implementation
    None

  be done_writing() =>
    // TODO: implementation
    None

  be print(data: ByteSeq) =>
    // TODO: implementation
    None

  be write(data: ByteSeq) =>
    // TODO: implementation
    None

  be printv(data: ByteSeqIter) =>
    // TODO: implementation
    None

  be writev(data: ByteSeqIter) =>
    // TODO: implementation
    None
