use @pony_os_errno[I32]()

primitive _STDINFILENO
  fun apply(): U32 => 0

primitive _STDOUTFILENO
  fun apply(): U32 => 1

primitive _STDERRFILENO
  fun apply(): U32 => 2

primitive _SIGKILL
  fun apply(): I32 => 9

primitive _SIGTERM
  fun apply(): I32 => 15

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

interface _Process
  fun kill()
  fun ref wait(): I32

class _ProcessNone is _Process
  fun kill() => None
  fun ref wait(): I32 => 0

class _ProcessPosix is _Process
  let pid: I32

  new create(path: String, args: Array[String] val, vars: Array[String] val, stdin: _Pipe, stdout: _Pipe, stderr: _Pipe) ? =>
    // prepare argp and envp ahead of fork() as it's not safe
    // to allocate in the child after fork() was called.
    let argp = _make_argv(args)
    let envp = _make_argv(vars)
    // fork child process
    pid = @fork[I32]()
    match pid
    | -1 => error
    | 0 => _child_fork(path, argp, envp, stdin, stdout, stderr)
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
    stdin: _Pipe, stdout: _Pipe, stderr: _Pipe)
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
    if 0 > @execve[I32](path.cstring(), argp.cpointer(),
      envp.cpointer())
    then
      @_exit[None](I32(-1))
    end

  fun _dup2(oldfd: U32, newfd: U32) =>
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
      if @kill[I32](pid, _SIGTERM()) < 0 then
        match @pony_os_errno()
        | _EINVAL() => None // Invalid argument, shouldn't happen but
                            // tryinng SIGKILL isn't likely to help.
        | _ESRCH() => None  // No such process, child has terminated
        else
          // Couldn't SIGTERM, as a last resort SIGKILL
          @kill[I32](pid, _SIGKILL())
        end
      end
    end

  fun ref wait(): I32 =>
    if pid > 0 then
      var wstatus: I32 = 0
      let options: I32 = 0
      if @waitpid[I32](pid, addressof wstatus, options) < 0 then
        -1
      else
        // extract the process exit code
        (wstatus >> 8) and 0xff
      end
    else
      -1
    end
