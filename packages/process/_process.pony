use "signals"
use "files"
use @pony_os_errno[I32]()
use @ponyint_wnohang[I32]() if posix
use @ponyint_pidfd_open[I32](pid: I32) if linux
use @ponyint_win_process_create[USize](appname: Pointer[U8] tag,
  cmdline: Pointer[U8] tag, environ: Pointer[U8] tag, wdir: Pointer[U8] tag,
  stdin_fd: U32, stdout_fd: U32, stderr_fd: U32,
  error_code: Pointer[U32], error_msg: Pointer[Pointer[U8]] tag)
use @ponyint_win_process_wait[I32](hProc: USize, code: Pointer[I32])
use @ponyint_win_process_kill[I32](hProc: USize)
use @execve[I32](path: Pointer[U8] tag, argp: Pointer[Pointer[U8] tag] tag,
  envp: Pointer[Pointer[U8] tag] tag)
use @fork[I32]()
use @chdir[I32](path: Pointer[U8] tag)
use @dup2[I32](fildes: U32, fildes2: U32)
use @write[ISize](fd: U32, buf: Pointer[U8] tag, size: USize)
use @kill[I32](pid_t: I32, sig: U32)
use @waitpid[I32](pid: I32, stat_loc: Pointer[I32] tag, opts: I32)
use @_exit[None](status: I32)

// for Windows System Error Codes see: https://docs.microsoft.com/de-de/windows/desktop/Debug/system-error-codes
primitive _ERRORBADEXEFORMAT
  fun apply(): U32 =>
    """
    ERROR_BAD_EXE_FORMAT
    %1 is not a valid Win32 application.
    """
    193 // 0xC1

primitive _ERRORDIRECTORY
  fun apply(): U32 =>
    """
    The directory name is invalid.
    """
    267 // 0x10B

primitive _STDINFILENO
  fun apply(): U32 => 0

primitive _STDOUTFILENO
  fun apply(): U32 => 1

primitive _STDERRFILENO
  fun apply(): U32 => 2

// Operation not permitted
primitive _EPERM
  fun apply(): I32 => 1

// No such process
primitive _ESRCH
  fun apply(): I32 => 3

// Interrupted function
primitive _EINTR
  fun apply(): I32 => 4

// Try again
primitive _EAGAIN
  fun apply(): I32 =>
    ifdef bsd or osx then 35
    elseif linux then 11
    elseif windows then 22
    else compile_error "no EAGAIN" end

// Invalid argument
primitive _EINVAL
  fun apply(): I32 => 22

// Function not implemented (kernel too old for pidfd_open)
primitive _ENOSYS
  fun apply(): I32 =>
    ifdef linux then 38
    else compile_error "no ENOSYS" end

primitive _EXOSERR
  fun apply(): I32 => 71

// For handling errors between @fork and @execve
primitive _StepChdir
  fun apply(): U8 => 1

primitive _StepExecve
  fun apply(): U8 => 2

primitive _WNOHANG
  fun apply(): I32 =>
    ifdef posix then
      @ponyint_wnohang()
    else
      compile_error "no clue what WNOHANG is on this platform."
    end

class val Exited is (Stringable & Equatable[ProcessExitStatus])
  """
  Process exit status: Process exited with an exit code.
  """
  let _exit_code: I32

  new val create(code: I32) =>
    _exit_code = code

  fun exit_code(): I32 =>
    """
    Retrieve the exit code of the exited process.
    """
    _exit_code

  fun string(): String iso^ =>
    recover iso
      String(10)
        .>append("Exited(")
        .>append(_exit_code.string())
        .>append(")")
    end

  fun eq(other: ProcessExitStatus): Bool =>
    match other
    | let e: Exited =>
      e._exit_code == _exit_code
    else
      false
    end

class val Signaled is (Stringable & Equatable[ProcessExitStatus])
  """
  Process exit status: Process was terminated by a signal.
  """
  let _signal: U32

  new val create(sig: U32) =>
    _signal = sig

  fun signal(): U32 =>
    """
    Retrieve the signal number that exited the process.
    """
    _signal

  fun string(): String iso^ =>
    recover iso
      String(12)
        .>append("Signaled(")
        .>append(_signal.string())
        .>append(")")
    end

  fun eq(other: ProcessExitStatus): Bool =>
    match other
    | let s: Signaled =>
      s._signal == _signal
    else
      false
    end


type ProcessExitStatus is (Exited | Signaled)
  """
  Representing possible exit states of processes.
  A process either exited with an exit code or, only on posix systems,
  has been terminated by a signal.
  """

primitive _StillRunning
  """
  The result of a non-blocking reap that found no exit to collect. From the
  start-up probe this means the child is still running. After an exit signal it
  means `waitpid` has not yet caught up to the OS's exit notification. What each
  caller does with it — wait, retry, or report failure — is the caller's own.
  """

type _WaitResult is (ProcessExitStatus | WaitpidError | _StillRunning)


interface _Process
  fun box kill()
  fun ref wait(): _WaitResult
    """
    Non-blocking reap of the child. Collects the exit status if the child has
    exited; does not block a scheduler thread.
    """
  fun ref arm_exit_event(owner: AsioEventNotify): AsioEventID
    """
    Register the child's native exit event with the asio backend and return its
    id. The event fires when the child exits.
    """
  fun ref close_exit_source(event: AsioEventID)
    """
    Unsubscribe the exit event and release the native exit source (the pidfd on
    Linux). Idempotent.
    """

class _ProcessPosix is _Process
  let pid: I32
  // Linux: a pidfd for the child, the exit source. BSD/macOS: -1, because
  // kqueue keys its EVFILT_PROC exit filter on the pid, not on an fd.
  var _exit_fd: I32 = -1

  new create(
    path: String,
    args: Array[String] val,
    vars: Array[String] val,
    wdir: (String val | None),
    err_far_fd: U32,
    stdin_far_fd: U32,
    stdout_far_fd: U32,
    stderr_far_fd: U32) ?
  =>
    // Prepare argp and envp ahead of fork() as it's not safe to allocate in
    // the child after fork() is called.
    let argp = _make_argv(args)
    let envp = _make_argv(vars)
    // Fork the child process, handling errors and the child fork case.
    pid = @fork()
    match pid
    | -1 => error
    | 0 =>
      _child_fork(path, argp, envp, wdir, err_far_fd, stdin_far_fd,
        stdout_far_fd, stderr_far_fd)
    end

    // Parent. Open the child's exit source. On Linux that is a pidfd; a
    // failure here is defensive (the factory already probed pidfd support), so
    // clean up the child we just forked rather than leaking it as a zombie.
    ifdef linux then
      let fd = @ponyint_pidfd_open(pid)
      if fd < 0 then
        // Force-kill and reap the child we forked but cannot monitor. SIGKILL,
        // not SIGTERM: the child may have already exec'd into a program that
        // ignores SIGTERM, and the reap below blocks. Retry on EINTR.
        @kill(pid, Sig.kill())
        var wstatus: I32 = 0
        while
          (@waitpid(pid, addressof wstatus, 0) < 0)
            and (@pony_os_errno() == _EINTR())
        do
          None
        end
        error
      end
      _exit_fd = fd
    end

  fun tag _make_argv(args: Array[String] box): Array[Pointer[U8] tag] =>
    """
    Convert an array of String parameters into an array of
    C pointers to same strings.
    """
    let argv = Array[Pointer[U8] tag](args.size() + 1)
    for s in args.values() do
      argv.push(s.cstring())
    end
    argv.push(Pointer[U8]) // nullpointer to terminate list of args
    argv

  fun _child_fork(
    path: String,
    argp: Array[Pointer[U8] tag],
    envp: Array[Pointer[U8] tag],
    wdir: (String val | None),
    err_far_fd: U32,
    stdin_far_fd: U32,
    stdout_far_fd: U32,
    stderr_far_fd: U32)
  =>
    """
    We are now in the child process. We redirect STDIN, STDOUT and STDERR
    to their pipes and execute the command. The command is executed via
    execve which does not return on success, and the text, data, bss, and
    stack of the calling process are overwritten by that of the program
    loaded. We've set the FD_CLOEXEC flag on all file descriptors to ensure
    that they are all closed automatically once @execve gets called.
    """
    _dup2(stdin_far_fd, _STDINFILENO())   // redirect stdin
    _dup2(stdout_far_fd, _STDOUTFILENO()) // redirect stdout
    _dup2(stderr_far_fd, _STDERRFILENO()) // redirect stderr

    var step: U8 = _StepChdir()

    match \exhaustive\ wdir
    | let d: String =>
      let dir: Pointer[U8] tag = d.cstring()
      if 0 > @chdir(dir) then
        @write(err_far_fd, addressof step, USize(1))
        @_exit(_EXOSERR())
      end
    | None => None
    end

    step = _StepExecve()
    if 0 > @execve(path.cstring(), argp.cpointer(),
      envp.cpointer())
    then
      @write(err_far_fd, addressof step, USize(1))
      @_exit(_EXOSERR())
    end

  fun tag _dup2(oldfd: U32, newfd: U32) =>
    """
    Creates a copy of the file descriptor oldfd using the file
    descriptor number specified in newfd. If the file descriptor newfd
    was previously open, it is silently closed before being reused.
    If dup2() fails because of EINTR we retry.
    """
    while (@dup2(oldfd, newfd) < 0) do
      if @pony_os_errno() == _EINTR() then
        continue
      else
        @_exit(I32(-1))
      end
    end

  fun box kill() =>
    """
    Terminate the process, first trying SIGTERM and if that fails, try SIGKILL.
    Only ever called while the child is an unreaped zombie we own, so its pid
    cannot have been recycled.
    """
    if pid > 0 then
      // Try a graceful termination
      if @kill(pid, Sig.term()) < 0 then
        match @pony_os_errno()
        | _EINVAL() => None // Invalid argument, shouldn't happen but
                            // trying SIGKILL isn't likely to help.
        | _ESRCH() => None  // No such process, child has terminated
        else
          // Couldn't SIGTERM, as a last resort SIGKILL
          @kill(pid, Sig.kill())
        end
      end
    end

  fun ref arm_exit_event(owner: AsioEventNotify): AsioEventID =>
    ifdef linux then
      // The pidfd goes readable when the child exits; register it as a plain
      // read event. epoll delivers it like any other fd.
      @pony_asio_event_create(owner, _exit_fd.u32(), AsioEvent.read(), 0, true)
    elseif bsd or osx then
      // kqueue's EVFILT_PROC keys on the pid; the backend reads ASIO_PROC and
      // registers NOTE_EXIT.
      @pony_asio_event_create(owner, pid.u32(), AsioEvent.proc(), 0, true)
    else
      compile_error "unsupported posix platform for process exit event"
    end

  fun ref close_exit_source(event: AsioEventID) =>
    // Unsubscribe before closing the fd, the same discipline _Pipe.close_near
    // uses: closing first would let another thread reuse the fd number before
    // we unsubscribe it here.
    if event isnt AsioEvent.none() then
      @pony_asio_event_unsubscribe(event)
    end
    ifdef linux then
      if _exit_fd != -1 then
        @close(_exit_fd.u32())
        _exit_fd = -1
      end
    end
    // BSD/macOS: no fd to close; the EVFILT_PROC knote is removed by the
    // unsubscribe above.

  fun ref wait(): _WaitResult =>
    """
    Non-blocking reap; retries waitpid on EINTR. We poll with WNOHANG and
    without WUNTRACED, so waitpid reports an exit but not a stop.
    """
    if pid > 0 then
      var wstatus: I32 = 0
      let options: I32 = _WNOHANG()
      var r: I32 = @waitpid(pid, addressof wstatus, options)
      while (r < 0) and (@pony_os_errno() == _EINTR()) do
        r = @waitpid(pid, addressof wstatus, options)
      end
      match \exhaustive\ r
      | let err: I32 if err < 0 =>
        WaitpidError
      | let exited_pid: I32 if exited_pid == pid => // our process changed state
        if _WaitPidStatus.exited(wstatus) then
          Exited(_WaitPidStatus.exit_code(wstatus))
        elseif _WaitPidStatus.signaled(wstatus) then
          Signaled(_WaitPidStatus.termsig(wstatus).u32())
        elseif _WaitPidStatus.stopped(wstatus) then
          // A stopped (not exited) child. We do not pass WUNTRACED, so waitpid
          // does not report a stop and this is unreached; if it ever were,
          // treat a stop as still running, never as an exit.
          _StillRunning
        elseif _WaitPidStatus.continued(wstatus) then
          _StillRunning
        else
          // An unrecognized wait status; report it as an error rather than
          // guess at an exit code.
          WaitpidError
        end
      | 0 => _StillRunning
      else
        WaitpidError
      end
    else
      WaitpidError
    end

primitive _WaitPidStatus
  """
  Pure Pony implementation of C macros for investigating
  the status returned by `waitpid()`.
  """

  fun exited(wstatus: I32): Bool =>
    termsig(wstatus) == 0

  fun exit_code(wstatus: I32): I32 =>
    (wstatus and 0xff00) >> 8

  fun signaled(wstatus: I32): Bool =>
    // Matches C's WIFSIGNALED: cast (termsig + 1) to a signed 8-bit value
    // before shifting, so the 0x7f (stopped/continued) forms overflow to a
    // negative value and are not counted as a terminating signal.
    ((termsig(wstatus) + 1).i8() >> 1) > 0

  fun termsig(wstatus: I32): I32 =>
    (wstatus and 0x7f)

  fun stopped(wstatus: I32): Bool =>
    (wstatus and 0xff) == 0x7f

  fun stopsig(wstatus: I32): I32 =>
    exit_code(wstatus)

  fun coredumped(wstatus: I32): Bool =>
    (wstatus and 0x80) != 0

  fun continued(wstatus: I32): Bool =>
    wstatus == 0xffff


class _ProcessWindows is _Process
  let h_process: USize
  let process_error: (ProcessError | None)
  var final_wait_result: (_WaitResult | None) = None

  new create(
    path: String,
    args: Array[String] val,
    vars: Array[String] val,
    wdir: (String val | None),
    stdin_far_fd: U32,
    stdout_far_fd: U32,
    stderr_far_fd: U32)
  =>
    ifdef windows then
      let wdir_ptr =
        match \exhaustive\ wdir
        | let wdir_str: String => wdir_str.cstring()
        | None => Pointer[U8] // NULL -> use parent directory
        end
      var error_code: U32 = 0
      var error_message = Pointer[U8]
      h_process = @ponyint_win_process_create(
          path.cstring(),
          _make_cmdline(args).cstring(),
          _make_environ(vars).cpointer(),
          wdir_ptr,
          stdin_far_fd, stdout_far_fd, stderr_far_fd,
          addressof error_code, addressof error_message)
      process_error =
        if h_process == 0 then
          match error_code
          | _ERRORBADEXEFORMAT() => ProcessError(ExecveError)
          | _ERRORDIRECTORY() =>
            let wdirpath =
              match \exhaustive\ wdir
              | let wdir_str: String => wdir_str
              | None => "?"
              end
            ProcessError(ChdirError, "Failed to change directory to "
              + wdirpath)
          else
            let message = String.from_cstring(error_message)
            ProcessError(ForkError, recover message.clone() end)
          end
        end
    else
      compile_error "unsupported platform"
    end

  fun tag _make_cmdline(args: Array[String] val): String =>
    var cmdline: String = ""
    for arg in args.values() do
      // quote args with spaces on Windows
      var next = arg
      ifdef windows then
        try
          if arg.contains(" ") and (not arg(0)? == '"') then
            next = "\"" + arg + "\""
          end
        end
      end
      cmdline = cmdline + next + " "
    end
    cmdline

  fun tag _make_environ(vars: Array[String] val): Array[U8] =>
    """
    Build the ANSI environment block for CreateProcess: each `name=value`
    string terminated by a null, then a final null ending the block. An empty
    environment is two nulls -- CreateProcess rejects a block that is a lone
    null.
    """
    var size: USize = if vars.size() == 0 then 2 else 1 end
    for varr in vars.values() do
      size = size + varr.size() + 1 // name=value\0
    end
    var environ = Array[U8](size)
    for varr in vars.values() do
      environ.append(varr)
      environ.push(0)
    end
    if vars.size() == 0 then
      environ.push(0)
    end
    environ.push(0)
    environ

  fun box kill() =>
    if h_process != 0 then
      @ponyint_win_process_kill(h_process)
    end

  fun ref arm_exit_event(owner: AsioEventNotify): AsioEventID =>
    ifdef windows then
      // Register a wait on the child's process handle. The handle rides in the
      // event's nsec slot; the readiness backend registers a wait on it and
      // fires ASIO_READ when the child exits. Arming the event hands the
      // handle's close to the backend (see close_exit_source and
      // ponyint_win_process_wait).
      @pony_asio_event_create(owner, 0, AsioEvent.proc(), h_process.u64(), true)
    else
      compile_error "unsupported platform"
    end

  fun ref close_exit_source(event: AsioEventID) =>
    ifdef windows then
      // Unsubscribe the exit event. The backend unregisters the wait and closes
      // the process handle, so there is nothing to close here. Idempotent:
      // unsubscribe is a no-op on an already-disposed event.
      if event isnt AsioEvent.none() then
        @pony_asio_event_unsubscribe(event)
      end
    else
      compile_error "unsupported platform"
    end

  fun ref wait(): _WaitResult =>
    match final_wait_result
    | let wr: _WaitResult =>
      wr
    else
      var wr: _WaitResult = WaitpidError
      if h_process != 0 then
        var exit_code: I32 = 0
        match \exhaustive\ @ponyint_win_process_wait(h_process, addressof exit_code)
        | 0 =>
          wr = Exited(exit_code)
          final_wait_result = wr
          wr
        | 1 => _StillRunning
        else
          // A wait error (the fixed -1 sentinel). We report WaitpidError,
          // matching the posix path.
          final_wait_result = wr
          wr
        end
      else
        final_wait_result = wr
        wr
      end
    end
