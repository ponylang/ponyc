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
primitive _FSETFL       fun apply(): I32 => 4
primitive _ONONBLOCK    fun apply(): I32 => 4

primitive ExecveError
primitive PipeError
primitive Dup2Error
primitive ForkError
primitive FcntlError
primitive WaitpidError
primitive CloseError
primitive ReadError
primitive WriteError
primitive Unsupported // we throw this on non POSIX systems

type ProcessError is
  ( ExecveError
  | PipeError    
  | Dup2Error    
  | ForkError
  | FcntlError
  | WaitpidError
  | CloseError
  | ReadError
  | WriteError
  | Unsupported
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
  
  new create(notifier: ProcessNotify iso, path: String, 
    args: Array[String] val, vars: Array[String] val) =>
    """
    Create infrastructure to communicate with a forked child process
    and register the asio events. Fork child process and notify our
    user about incoming data via the notifier. 
    """
    _notifier = consume notifier

    ifdef posix then
      try
        (_stdin_read, _stdin_write)   = _make_pipe()
        (_stdout_read, _stdout_write) = _make_pipe()
        (_stderr_read, _stderr_write) = _make_pipe()
      else
        @close[I32](_stdin_read)
        @close[I32](_stdin_write)
        @close[I32](_stdout_read)
        @close[I32](_stdout_write)
        @close[I32](_stderr_read)
        @close[I32](_stderr_write)
        _notifier.failed(PipeError)
      end
      // fork child process
      _child_pid = @fork[I32]()
      match _child_pid
      | -1  => _notifier.failed(ForkError)
      | 0   => _child(path, args, vars)
      else
        _parent()
      end
    elseif windows then
      _notifier.failed(Unsupported)
    else
      compile_error "unsupported platform"
    end

    
  fun _child(path: String, args: Array[String] val, vars: Array[String] val) =>
    """
    We're now in the child process. We redirect STDIN, STDOUT and STDERR
    to their pipes and execute the command. The command is executed via
    execve which does not return on success, and the text, data, bss, and
    stack of the calling process are overwritten by that of the program
    loaded.
    """
    ifdef posix then
      _dup2(_stdin_read, _STDINFILENO())    // redirect stdin
      _dup2(_stdout_write, _STDOUTFILENO()) // redirect stdout
      _dup2(_stderr_write, _STDERRFILENO()) // redirect stderr
      // close our ends of all three pipes
      @close[I32](_stdin_read)
      @close[I32](_stdin_write)
      @close[I32](_stdout_read)
      @close[I32](_stdout_write)
      @close[I32](_stderr_read)
      @close[I32](_stderr_write)
      // prep and execute
      let argp = _make_argv(args)
      let envp = _make_argv(vars)
      let pathp = path.cstring()
      if @execve[I32](pathp, argp.cstring(), envp.cstring()) < 0 then
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
      try
        _set_o_nonblock(_stdout_read)
        _set_o_nonblock(_stderr_read)
      else
        _notifier.failed(FcntlError)
        _close()
      end
      @close[I32](_stdin_read)
      @close[I32](_stdout_write)
      @close[I32](_stderr_write)
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
    TODO:
    - If dup2() fails because of EINTR we should retry.
    """
    ifdef posix then
      if @dup2[I32](oldfd, newfd) < 0 then
        @exit[None](I32(-1))
      end
    end

    
  fun _make_pipe(): (U32, U32) ? =>
    """
    Creates a pipe, an unidirectional data channel that can be used
    for interprocess communication.
    """
    ifdef posix then  
      var pipe = (U32(0), U32(0))
      if @pipe[I32](addressof pipe) < 0 then
        error
      end
      pipe
    else
      (U32(0), U32(0))
    end


  be print(data: ByteSeq) =>
    """
    Print some bytes and insert a newline afterwards.
    """
    write(data)
    write("\n")


  be write(data: ByteSeq) =>
    """
    Write to STDIN of the child process.
    """
    ifdef posix then
      let d = data
      //write to the STDIN pipe of our child
      if @write[USize](_stdin_write, d.cstring(), d.size()) < 0 then
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
    
  be dispose() =>
    """
    Close all pipes to the forked process and wait for the child to exit.
    """
    _close()


  fun _create_asio_event(fd: U32): AsioEventID =>
    """
    Takes a file descriptor (one end of a pipe) and returns and AsioEvent.
    """
    ifdef posix then
      @pony_asio_event_create(this, fd, AsioEvent.read(), 0, true)
    else
      AsioEvent.none()
    end


  fun _set_o_nonblock(fd: U32) ? =>
    """
    Set the O_NONBLOCK flag on a file descriptor to enable async operations.
    """
    ifdef posix then
      if @fcntl[I32](fd, _FSETFL(), _ONONBLOCK()) < 0 then
        error
      end
    end

  
  be _event_notify(event: AsioEventID, flags: U32, arg: U32) =>
    """
    Handle the incoming event.
    """
    match event
    | _stdout_event =>
      if AsioEvent.readable(flags) then
        _stdout_open = _pending_reads(_stdout_read)
      elseif
        AsioEvent.disposable(flags) then
        @pony_asio_event_destroy(event)
        _stdout_event = AsioEvent.none()
      end
    | _stderr_event =>
      if AsioEvent.readable(flags) then
        _stderr_open = _pending_reads(_stderr_read)
      elseif
        AsioEvent.disposable(flags) then
        @pony_asio_event_destroy(event)
        _stderr_event = AsioEvent.none()
      end
    end
    _try_shutdown()

    
  fun ref _close() =>
    """ 
    Close all pipes, unsubscribe events and wait for the child process to exit.
    """
    ifdef posix then
      if not _closed then
        _closed = true
        @close[I32](_stdin_read)
        @close[I32](_stdin_write)
        @close[I32](_stdout_read)
        @close[I32](_stdout_write)
        @close[I32](_stderr_read)
        @close[I32](_stderr_write)
        _stdout_open = false
        _stderr_open = false
        @pony_asio_event_unsubscribe(_stdout_event)
        @pony_asio_event_unsubscribe(_stderr_event)
      end
      // We want to capture the exit status of the child
      var wstatus: I32 = 0
      let options: I32 = 0
      if @waitpid[USize](_child_pid, addressof wstatus, options) < 0 then
        _notifier.failed(WaitpidError)
      end
      // process child exit code
      _notifier.dispose((wstatus >> 8) and 0xff)
    end
    
  fun ref _try_shutdown() =>
    """
    If neither stdout nor stderr are open we close down and exit.
      """
    if _stdout_open or _stderr_open then
      return
    end
    _close()

    
  fun ref _pending_reads(fd: U32): Bool =>
    """
    Read from stdout while data is available. If we read 4 kb of data, 
    send ourself a resume message and stop reading, to avoid starving 
    other actors.
    It's safe to use the same buffer for stdout and stderr because of
    causal messaging. Events get processed one _after_ another.
    """
    ifdef posix then
      var sum: USize = 0
      while true do
        let len = @read[ISize](fd, _read_buf.cstring(), _read_buf.space())
        let errno = @pony_os_errno()
        let next = _read_buf.space()
        match len
        | -1 => match errno
                | _EAGAIN() =>  return true // resource temporarily unavailable, retry
                else
                  return false
                end
        | 0  => match errno
                | _EAGAIN() =>  return true // resource temporarily unavailable, retry
                else
                  return false
                end
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
  
    



  
