use "signals"
use "files"
use @pony_os_errno[I32]()
use @pony_os_clear_errno[None]()

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

primitive _EXOSERR
  fun apply(): I32 => 71

// For handling errors between @fork and @execve
primitive _StepChdir
  fun apply(): U8 => 1

primitive _StepExecve
  fun apply(): U8 => 2

primitive WNOHANG
  fun apply(): I32 =>
    ifdef posix then
      @ponyint_wnohang[I32]()
    else
      compile_error "no clue what WNOHANG is on this platform."
    end

class val _ExitCode
  let code: I32

  new val create(code': I32) =>
    code = code'

class val _WaitPidError
  let error_code: I32

  new val create(code: I32) =>
    error_code = code

  new val unknown_error() =>
    error_code = -1

primitive _StillRunning
type _WaitResult is (_ExitCode | _WaitPidError | _StillRunning)


interface _Process
  fun kill()
  fun ref wait(): _WaitResult
    """
    Only polls, does not actually wait for the process to finish,
    in order to not block a scheduler thread.
    """


class _ProcessNone is _Process
  fun kill() => None
  fun ref wait(): _WaitResult => _ExitCode(0)

class _ProcessPosix is _Process
  let pid: I32

  new create(
    path: String,
    args: Array[String] val,
    vars: Array[String] val,
    wdir: (FilePath | None),
    err: _Pipe,
    stdin: _Pipe,
    stdout: _Pipe,
    stderr: _Pipe) ?
  =>
    // Prepare argp and envp ahead of fork() as it's not safe to allocate in
    // the child after fork() is called.
    let argp = _make_argv(args)
    let envp = _make_argv(vars)
    // Fork the child process, handling errors and the child fork case.
    pid = @fork[I32]()
    match pid
    | -1 => error
    | 0 => _child_fork(path, argp, envp, wdir, err, stdin, stdout, stderr)
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
    wdir: (FilePath | None),
    err: _Pipe, stdin: _Pipe, stdout: _Pipe, stderr: _Pipe)
  =>
    """
    We are now in the child process. We redirect STDIN, STDOUT and STDERR
    to their pipes and execute the command. The command is executed via
    execve which does not return on success, and the text, data, bss, and
    stack of the calling process are overwritten by that of the program
    loaded. We've set the FD_CLOEXEC flag on all file descriptors to ensure
    that they are all closed automatically once @execve gets called.
    """
    _dup2(stdin.far_fd, _STDINFILENO())   // redirect stdin
    _dup2(stdout.far_fd, _STDOUTFILENO()) // redirect stdout
    _dup2(stderr.far_fd, _STDERRFILENO()) // redirect stderr

    var step: U8 = _StepChdir()

    match wdir
    | let d: FilePath =>
      let dir: Pointer[U8] tag = d.path.cstring()
      if 0 > @chdir[I32](dir) then
        @write[ISize](err.far_fd, addressof step, USize(1))
        @_exit[None](_EXOSERR())
      end
    | None => None
    end

    step = _StepExecve()
    if 0 > @execve[I32](path.cstring(), argp.cpointer(),
      envp.cpointer())
    then
      @write[ISize](err.far_fd, addressof step, USize(1))
      @_exit[None](_EXOSERR())
    end

  fun tag _dup2(oldfd: U32, newfd: U32) =>
    """
    Creates a copy of the file descriptor oldfd using the file
    descriptor number specified in newfd. If the file descriptor newfd
    was previously open, it is silently closed before being reused.
    If dup2() fails because of EINTR we retry.
    """
    while (@dup2[I32](oldfd, newfd) < 0) do
      if @pony_os_errno() == _EINTR() then
        continue
      else
        @_exit[None](I32(-1))
      end
    end

  fun kill() =>
    """
    Terminate the process, first trying SIGTERM and if that fails, try SIGKILL.
    """
    if pid > 0 then
      // Try a graceful termination
      if @kill[I32](pid, Sig.term()) < 0 then
        match @pony_os_errno()
        | _EINVAL() => None // Invalid argument, shouldn't happen but
                            // tryinng SIGKILL isn't likely to help.
        | _ESRCH() => None  // No such process, child has terminated
        else
          // Couldn't SIGTERM, as a last resort SIGKILL
          @kill[I32](pid, Sig.kill())
        end
      end
    end

  fun ref wait(): _WaitResult =>
    """Only polls, does not block."""
    if pid > 0 then
      var wstatus: I32 = 0
      let options: I32 = 0 or WNOHANG()
      // poll, do not block
      match @waitpid[I32](pid, addressof wstatus, options)
      | let err: I32 if err < 0 =>
        let wpe = _WaitPidError(@pony_os_errno())
        @pony_os_clear_errno()
        wpe
      | let exited_pid: I32 if exited_pid == pid => // our process finished
        _ExitCode((wstatus >> 8) and 0xff)
      | 0 => _StillRunning
      else
        _WaitPidError.unknown_error()
      end
    else
      _WaitPidError.unknown_error()
    end

class _ProcessWindows is _Process
  let hProcess: USize
  let processError: (ProcessError | None)

  new create(
    path: String,
    args: Array[String] val,
    vars: Array[String] val,
    wdir: (FilePath | None),
    stdin: _Pipe,
    stdout: _Pipe,
    stderr: _Pipe)
  =>
    ifdef windows then
      let wdir_ptr =
        match wdir
        | let wdir_fp: FilePath => wdir_fp.path.cstring()
        | None => Pointer[U8] // NULL -> use parent directory
        end
      var error_code: U32 = 0
      var error_message = Pointer[U8]
      hProcess = @ponyint_win_process_create[USize](
          path.cstring(),
          _make_cmdline(args).cstring(),
          _make_environ(vars).cpointer(),
          wdir_ptr,
          stdin.far_fd, stdout.far_fd, stderr.far_fd,
          addressof error_code, addressof error_message)
      processError =
        if hProcess == 0 then
          match error_code
          | _ERRORBADEXEFORMAT() => ProcessError(ExecveError)
          | _ERRORDIRECTORY() =>
            let wdirpath =
              match wdir
              | let wdir_fp: FilePath => wdir_fp.path
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
    var size: USize = 0
    for varr in vars.values() do
      size = size + varr.size() + 1 // name=value\0
    end
    size = size + 1 // last \0
    var environ = Array[U8](size)
    for varr in vars.values() do
      environ.append(varr)
      environ.push(0)
    end
    environ.push(0)
    environ

  fun kill() =>
    if hProcess != 0 then
      @ponyint_win_process_kill[I32](hProcess)
    end

  fun ref wait(): _WaitResult =>
    if hProcess != 0 then
      var exit_code: I32 = 0
      match @ponyint_win_process_wait[I32](hProcess, addressof exit_code)
      | 0 => _ExitCode(exit_code)
      | 1 => _StillRunning
      | let code: I32 =>
        @printf[None]("_ProcessWindows wait returned: ".cstring(), code.string().cstring())
        _WaitPidError(code)
      end
    else
      _WaitPidError.unknown_error()
    end

