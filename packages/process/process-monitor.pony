"""
# Process package

The Process package provides support for handling Unix style processes.
For each external process that you want to handle, you need to create a 
`ProcessMonitor` and a corresponding `ProcessNotify` object. Each ProcessMonitor
runs as it own actor and upon receiving data will call its corresponding 
`ProcessNotify`'s method.

## Example program

The following program will spawn an external program and write to it's
STDIN. Output received on STDOUT of the child process is forwarded to the
ProcessNotify client and printed.

```pony
use "process"

actor Main
  let _env: Env
  
  new create(env: Env) =>
    _env = env
    // create a notifier
    let client = ProcessClient(_env)
    let notifier: ProcessNotify iso = consume client
    // define the binary to run
    let path: String = "/bin/cat"
    // define the arguments
    let args: Array[String] iso = recover Array[String](1) end
    args.push("cat")
    // define the environment variable for the execution
    let vars: Array[String] iso = recover Array[String](2) end
    vars.push("HOME=/")
    vars.push("PATH=/bin")
    // create a ProcessMonitor and spawn the child process
    let pm: ProcessMonitor = ProcessMonitor(consume notifier, path,
      consume args, consume vars)
    // write to STDIN of the child process
    pm.write("one, two, three")
    pm.done_writing() // closing stdin allows cat to terminate
    // dispose of the ProcessMonitor


// define a client that implements the ProcessNotify interface
class ProcessClient is ProcessNotify
  let _env: Env
  
  new iso create(env: Env) =>
    _env = env
    
  fun ref stdout(data: Array[U8] iso) =>
    let out = String.from_array(consume data)
    _env.out.print("STDOUT: " + out)

  fun ref stderr(data: Array[U8] iso) =>
    let err = String.from_array(consume data)
    _env.out.print("STDERR: " + err)
    
  fun ref failed(err: ProcessError) =>
    match err
    | ExecveError   => _env.out.print("ProcessError: ExecveError")
    | PipeError     => _env.out.print("ProcessError: PipeError")
    | Dup2Error     => _env.out.print("ProcessError: Dup2Error")
    | ForkError     => _env.out.print("ProcessError: ForkError")
    | FcntlError    => _env.out.print("ProcessError: FcntlError")
    | WaitpidError  => _env.out.print("ProcessError: WaitpidError")
    | CloseError    => _env.out.print("ProcessError: CloseError")
    | ReadError     => _env.out.print("ProcessError: ReadError")
    | WriteError    => _env.out.print("ProcessError: WriteError")
    | KillError     => _env.out.print("ProcessError: KillError")
    | Unsupported   => _env.out.print("ProcessError: Unsupported") 
    else
      _env.out.print("Unknown ProcessError!")
    end
    
  fun ref dispose(child_exit_code: I32) =>
    let code: I32 = consume child_exit_code
    _env.out.print("Child exit code: " + code.string())
```

## Process portability

The ProcessMonitor supports spawning processes on Linux, FreeBSD and OSX. 
Processes are not supported on Windows and attempting to use them will cause 
a runtime error.

## Shutting down ProcessMonitor and external process

Document waitpid behaviour (stops world)

"""
use "files"
use @pony_os_errno[I32]()
use @pony_asio_event_create[AsioEventID](owner: AsioEventNotify, fd: U32, 
      flags: U32, nsec: U64, noisy: Bool)
use @pony_asio_event_unsubscribe[None](event: AsioEventID)
use @pony_asio_event_destroy[None](event: AsioEventID)
      
primitive _EINTR        fun apply(): I32 => 4
primitive _EAGAIN       fun apply(): I32 => 35
primitive _STDINFILENO  fun apply(): U32 => 0
primitive _STDOUTFILENO fun apply(): U32 => 1
primitive _STDERRFILENO fun apply(): U32 => 2
primitive _ONONBLOCK    fun apply(): I32 => 4
primitive _FSETFL       fun apply(): I32 => 4
primitive _FGETFL       fun apply(): I32 => 3
primitive _FSETFD       fun apply(): I32 => 2
primitive _FGETFD       fun apply(): I32 => 1
primitive _FDCLOEXEC    fun apply(): I32 => 1
primitive _SIGTERM      fun apply(): I32 => 15

primitive ExecveError
primitive PipeError
primitive Dup2Error
primitive ForkError
primitive FcntlError
primitive WaitpidError
primitive CloseError
primitive ReadError
primitive WriteError
primitive KillError
primitive Unsupported // we throw this on non POSIX systems
primitive CapError

type ProcessError is
  ( ExecveError
  | CloseError
  | Dup2Error    
  | FcntlError
  | ForkError
  | KillError
  | PipeError    
  | ReadError
  | Unsupported
  | WaitpidError
  | WriteError
  | CapError
  )    
  
actor ProcessMonitor
  """
  Forks and monitors a process. Notifies a client about STDOUT / STDERR events.
  """        
  let _notifier: ProcessNotify
  var _child_pid: I32 = -1
  
  var _stdout_event: AsioEventID = AsioEvent.none()
  var _stderr_event: AsioEventID = AsioEvent.none()

  var _read_buf: Array[U8] iso = recover Array[U8].undefined(4096) end

  var _stdin_read:   U32 = -1
  var _stdin_write:  U32 = -1
  var _stdout_read:  U32 = -1
  var _stdout_write: U32 = -1
  var _stderr_read:  U32 = -1
  var _stderr_write: U32 = -1
    
  var _stdout_open: Bool = true
  var _stderr_open: Bool = true

  var _closed: Bool = false
  
  new create(notifier: ProcessNotify iso, filepath: FilePath, 
    args: Array[String] val, vars: Array[String] val)
  =>
    """
    Create infrastructure to communicate with a forked child process
    and register the asio events. Fork child process and notify our
    user about incoming data via the notifier. 
    """
    _notifier = consume notifier
    if not filepath.caps(FileExec) then
      _notifier.failed(CapError)
      return
    end

    ifdef posix then
      try
        (_stdin_read, _stdin_write)   = _make_pipe(_FDCLOEXEC(), _ONONBLOCK())
        (_stdout_read, _stdout_write) = _make_pipe(_FDCLOEXEC(), _ONONBLOCK())
        (_stderr_read, _stderr_write) = _make_pipe(_FDCLOEXEC(), _ONONBLOCK())
      else
        _close_fd(_stdin_read)
        _close_fd(_stdin_write)
        _close_fd(_stdout_read)
        _close_fd(_stdout_write)
        _close_fd(_stderr_read)
        _close_fd(_stderr_write)
        _notifier.failed(PipeError)
        return
      end
      // prepare argp and envp ahead of fork() as it's not safe
      // to allocate in the child after fork() was called.
      let argp = _make_argv(args)
      let envp = _make_argv(vars)
      // fork child process
      _child_pid = @fork[I32]()
      match _child_pid
      | -1  => _notifier.failed(ForkError)
      | 0   => _child(filepath.path, argp, envp)
      else
        _parent()
      end
    elseif windows then
      _notifier.failed(Unsupported)
    else
      compile_error "unsupported platform"
    end
    
  fun _child(path: String, argp: Array[Pointer[U8] tag],
    envp: Array[Pointer[U8] tag])
    =>
    """
    We are now in the child process. We redirect STDIN, STDOUT and STDERR
    to their pipes and execute the command. The command is executed via
    execve which does not return on success, and the text, data, bss, and
    stack of the calling process are overwritten by that of the program
    loaded. We've set the FD_CLOEXEC flag on all file descriptors to ensure
    that they are all closed automatically once @execve gets called.
    """
    ifdef posix then
      _dup2(_stdin_read, _STDINFILENO())    // redirect stdin
      _dup2(_stdout_write, _STDOUTFILENO()) // redirect stdout
      _dup2(_stderr_write, _STDERRFILENO()) // redirect stderr
      if @execve[I32](path.cstring(), argp.cstring(), envp.cstring()) < 0 then
        @exit[None](I32(-1))
      end
    end
      
  fun ref _parent() =>
    """
    We're now in the parent process. We setup asio events for STDOUT and STDERR
    and close the file descriptors we don't need.
    """
    ifdef posix then
      _stdout_event = _create_asio_event(_stdout_read)
      _stderr_event = _create_asio_event(_stderr_read)
      _close_fd(_stdin_read)
      _close_fd(_stdout_write)
      _close_fd(_stderr_write)
    end

  fun _make_argv(args: Array[String] box): Array[Pointer[U8] tag] =>
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

  fun _dup2(oldfd: U32, newfd: U32) =>
    """
    Creates a copy of the file descriptor oldfd using the file
    descriptor number specified in newfd. If the file descriptor newfd
    was previously open, it is silently closed before being reused.
    If dup2() fails because of EINTR we retry.
    """
    ifdef posix then
      while (@dup2[I32](oldfd, newfd) < 0) do
        if @pony_os_errno() == _EINTR() then
          continue
        else
          @exit[None](I32(-1))
        end
      end
    end
    
  fun _make_pipe(fd_flags: I32, fl_flags: I32): (U32, U32) ? =>
    """
    Creates a pipe, an unidirectional data channel that can be used
    for interprocess communication. We need to set the flags O_NONBLOCK
    and O_CLOEXEC on the two file descriptors here to prevent capturing
    by another thread.
    """
    ifdef posix then
      var pipe = (U32(0), U32(0))
      if @pipe[I32](addressof pipe) < 0 then
        error
      end
      _set_fd(pipe._1, fd_flags)
      _set_fl(pipe._1, fl_flags)
      _set_fd(pipe._2, fd_flags)
      _set_fl(pipe._2, fl_flags)
      pipe
    else
      (U32(0), U32(0))
    end

  fun _getflags(fd: U32): String box ? =>
    """
    Get the string representation of flags set for a file descriptor
    """
    let result: String ref = String
    ifdef posix then
      let fd_flags: I32 = @fcntl[I32](fd, _FGETFD(), I32(0))
      if fd_flags < 0 then error end
      if (fd_flags and _FDCLOEXEC()) > 0 then
        result.append(" _FDCLOEXEC ")
      end
      let fl_flags: I32 = @fcntl[I32](fd, _FGETFL(), I32(0))
      if fl_flags < 0 then error end
        if (fl_flags and _ONONBLOCK()) > 0 then
          result.append(" O_NONBLOCK ")
        end
      result
    else
      result
    end

  fun _set_fd(fd: U32, flags: I32) ? =>
    let result = @fcntl[I32](fd, _FSETFD(), flags)
    if result < 0 then error end

  fun _set_fl(fd: U32, flags: I32) ? =>
    let result = @fcntl[I32](fd, _FSETFL(), flags)
    if result < 0 then error end
    
  be print(data: ByteSeq) =>
    """
    Print some bytes and insert a newline afterwards.
    """
    write(data)
    write("\n")

  be write(data: ByteSeq) =>
    """
    Write to STDIN of the child process.
    TODO: Signal back success/failure
    """
    ifdef posix then
      let d = data
      if _stdin_write > 0 then
        let res = @write[ISize](_stdin_write, d.cstring(), d.size())
        if res < 0 then
          _notifier.failed(WriteError)
        end
      else
        _notifier.failed(WriteError)
      end
    end

  be printv(data: ByteSeqIter) =>
    """
    Print an iterable collection of ByteSeqs.
    """
    for bytes in data.values() do
      print(bytes)
    end
    
  be writev(data: ByteSeqIter) =>
    """
    Write an iterable collection of ByteSeqs.
    """
    for bytes in data.values() do
      write(bytes)
    end
    
  be done_writing() =>
    """
    Close _stdin_write file descriptor.
    """
    _close_fd(_stdin_write)

  be dispose() =>
    """
    Terminate child and close down everything.
    """
    try
      _kill_child()
    else
      _notifier.failed(KillError)
      return
    end
    _close()

  fun _kill_child() ? =>
    """
    Terminate the child process.
    """
    if @kill[I32](_child_pid, _SIGTERM()) < 0 then error end
    
  fun _create_asio_event(fd: U32): AsioEventID =>
    """
    Takes a file descriptor (one end of a pipe) and returns an AsioEvent.
    """
    ifdef posix then
      @pony_asio_event_create(this, fd, AsioEvent.read(), 0, true)
    else
      AsioEvent.none()
    end

  fun _event_flags(flags: U32): String box=>
    """
    Return all flags of an event as a string.
    """
    let all: String ref = String
    if AsioEvent.readable(flags) then all.append("readable|") end
    if AsioEvent.disposable(flags) then all.append("disposable|") end
    if AsioEvent.writeable(flags) then all.append("writeable|") end
    all
    
  be _event_notify(event: AsioEventID, flags: U32, arg: U32) =>
    """
    Handle the incoming event.
    """
    match event
    | _stdout_event =>
      if AsioEvent.readable(flags) then
        _stdout_open = _pending_reads(_stdout_read)
      elseif AsioEvent.disposable(flags) then
        @pony_asio_event_destroy(event)
        _stdout_event = AsioEvent.none()
      end
    | _stderr_event =>
      if AsioEvent.readable(flags) then
        _stderr_open = _pending_reads(_stderr_read)
      elseif AsioEvent.disposable(flags) then
        @pony_asio_event_destroy(event)
        _stderr_event = AsioEvent.none()
      end
    end
    _try_shutdown()

  fun ref _close_fd(fd: U32) =>
    """
    Close a file descriptor.
    """
    @close[I32](fd)
    match fd
    | _stdin_read   => _stdin_read   = -1
    | _stdin_write  => _stdin_write  = -1
    | _stdout_read  => _stdout_read  = -1
    | _stdout_write => _stdout_write = -1
    | _stderr_read  => _stderr_read  = -1
    | _stderr_write => _stderr_write = -1
    end
    
  fun ref _close() =>
    """ 
    Close all pipes, unsubscribe events and wait for the child process to exit.
    """
    ifdef posix then
      if not _closed then
        _closed = true
        _close_fd(_stdin_read)
        _close_fd(_stdin_write)
        _close_fd(_stdout_read)
        _close_fd(_stdout_write)
        _close_fd(_stderr_read)
        _close_fd(_stderr_write)
        _stdout_open = false
        _stderr_open = false
        @pony_asio_event_unsubscribe(_stdout_event)
        @pony_asio_event_unsubscribe(_stderr_event)
        // We want to capture the exit status of the child
        var wstatus: I32 = 0
        let options: I32 = 0
        if @waitpid[I32](_child_pid, addressof wstatus, options) < 0 then
          _notifier.failed(WaitpidError)
        end
        // process child exit code
        _notifier.dispose((wstatus >> 8) and 0xff)
      end
    end
    
  fun ref _try_shutdown() =>
    """
    If neither stdout nor stderr are open we close down and exit.
    """
    if (_stdout_read == -1) and (_stderr_read == -1) then
      _close()
    end
    
  fun ref _pending_reads(fd: U32): Bool =>
    """
    Read from stdout while data is available. If we read 4 kb of data, 
    send ourself a resume message and stop reading, to avoid starving 
    other actors.
    It's safe to use the same buffer for stdout and stderr because of
    causal messaging. Events get processed one _after_ another.
    """
    ifdef posix then
      if fd == -1 then return false end
      var sum: USize = 0
      while true do
        let len = @read[ISize](fd, _read_buf.cstring(), _read_buf.space())
        let errno = @pony_os_errno()
        let next = _read_buf.space()
        match len
        | -1 =>
          if errno == _EAGAIN()  then
            return true // resource temporarily unavailable, retry
          end
          _close_fd(fd)
          return false          
        | 0  =>
          _close_fd(fd)
          return false
        else
          let data = _read_buf = recover Array[U8].undefined(next) end
          data.truncate(len.usize())
          match fd
          | _stdout_read => _notifier.stdout(consume data)
          | _stderr_read => _notifier.stderr(consume data)
          end
          sum = sum + len.usize()
          if sum > (1 << 12) then
            // If we've read 4 kb, yield and read again later.
            _read_again(fd)
            return true
          end
        end
      end
      true
    else
      true
    end
    
  be _read_again(fd: U32) =>
    """
    Resume reading on file descriptor.
    """
    match fd
    | _stdout_read => _stdout_open = _pending_reads(fd)
    | _stderr_read => _stderr_open = _pending_reads(fd)
    end
