"""
# Process package

The Process package provides support for handling Unix style processes.
For each external process that you want to handle, you need to create a
`ProcessMonitor` and a corresponding `ProcessNotify` object. Each
ProcessMonitor runs as it own actor and upon receiving data will call its
corresponding `ProcessNotify`'s method.

## Example program

The following program will spawn an external program and write to it's
STDIN. Output received on STDOUT of the child process is forwarded to the
ProcessNotify client and printed.

```pony
use "process"
use "files"

actor Main
  new create(env: Env) =>
    // create a notifier
    let client = ProcessClient(env)
    let notifier: ProcessNotify iso = consume client
    // define the binary to run
    try
      let path = FilePath(env.root as AmbientAuth, "/bin/cat")?
      // define the arguments; first arg is always the binary name
      let args: Array[String] val = ["cat"]
      // define the environment variable for the execution
      let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]
      // create a ProcessMonitor and spawn the child process
      let auth = env.root as AmbientAuth
      let pm: ProcessMonitor = ProcessMonitor(auth, auth, consume notifier,
      path, args, vars)
      // write to STDIN of the child process
      pm.write("one, two, three")
      pm.done_writing() // closing stdin allows cat to terminate
    else
      env.out.print("Could not create FilePath!")
    end

// define a client that implements the ProcessNotify interface
class ProcessClient is ProcessNotify
  let _env: Env

  new iso create(env: Env) =>
    _env = env

  fun ref stdout(process: ProcessMonitor ref, data: Array[U8] iso) =>
    let out = String.from_array(consume data)
    _env.out.print("STDOUT: " + out)

  fun ref stderr(process: ProcessMonitor ref, data: Array[U8] iso) =>
    let err = String.from_array(consume data)
    _env.out.print("STDERR: " + err)

  fun ref failed(process: ProcessMonitor ref, err: ProcessError) =>
    match err
    | ExecveError => _env.out.print("ProcessError: ExecveError")
    | PipeError => _env.out.print("ProcessError: PipeError")
    | ForkError => _env.out.print("ProcessError: ForkError")
    | WaitpidError => _env.out.print("ProcessError: WaitpidError")
    | WriteError => _env.out.print("ProcessError: WriteError")
    | KillError => _env.out.print("ProcessError: KillError")
    | CapError => _env.out.print("ProcessError: CapError")
    | Unsupported => _env.out.print("ProcessError: Unsupported")
    else _env.out.print("Unknown ProcessError!")
    end

  fun ref dispose(process: ProcessMonitor ref, child_exit_code: I32) =>
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
use "backpressure"
use "collections"
use "files"
use "debug"
use "time"

primitive ExecveError
primitive PipeError
primitive ForkError
primitive WaitpidError
primitive WriteError
primitive KillError   // Not thrown at this time
primitive Unsupported // we throw this on non POSIX systems
primitive CapError

type ProcessError is
  ( ExecveError
  | ForkError
  | KillError
  | PipeError
  | Unsupported
  | WaitpidError
  | WriteError
  | CapError
  )

type ProcessMonitorAuth is (AmbientAuth | StartProcessAuth)

actor ProcessMonitor
  """
  Fork+execs / creates a child process and monitors it. Notifies a client about STDOUT / STDERR events.
  """
  let _notifier: ProcessNotify
  let _backpressure_auth: BackpressureAuth

  var _stdin: _Pipe = _Pipe.none()
  var _stdout: _Pipe = _Pipe.none()
  var _stderr: _Pipe = _Pipe.none()
  var _child: _Process = _ProcessNone

  let _max_size: USize = 4096
  var _read_buf: Array[U8] iso = recover Array[U8] .> undefined(_max_size) end
  var _read_len: USize = 0
  var _expect: USize = 0
  embed _pending: List[(ByteSeq, USize)] = _pending.create()

  var _stdin_writeable: Bool = true
  var _stdout_readable: Bool = true  // TODO: written, but never read
  var _stderr_readable: Bool = true  // TODO: written, but never read

  var _done_writing: Bool = false

  var _closed: Bool = false
  var _timers: (Timers tag | None) = None  // For windows only

  new create(
    auth: ProcessMonitorAuth,
    backpressure_auth: BackpressureAuth,
    notifier: ProcessNotify iso,
    filepath: FilePath,
    args: Array[String] val,
    vars: Array[String] val)
  =>
    """
    Create infrastructure to communicate with a forked child process
    and register the asio events. Fork child process and notify our
    user about incoming data via the notifier.
    """
    _backpressure_auth = backpressure_auth
    _notifier = consume notifier

    // We need permission to execute and the
    // file itself needs to be an executable
    if not filepath.caps(FileExec) then
      _notifier.failed(this, CapError)
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
      _notifier.failed(this, ExecveError)
      return
    end

    try
      _stdin = _Pipe(true)?
      _stdout = _Pipe(false)?
      _stderr = _Pipe(false)?
    else
      _stdin.close()
      _stdout.close()
      _stderr.close()
      _notifier.failed(this, PipeError)
      return
    end

    try
      ifdef posix then
        _child = _ProcessPosix.create(filepath.path, args, vars, _stdin, _stdout, _stderr)?
      elseif windows then
        _child = _ProcessWindows.create(filepath.path, args, vars, _stdin, _stdout, _stderr)
      else
        compile_error "unsupported platform"
      end
      _stdin.begin(this, true)
      _stdout.begin(this, false)
      _stderr.begin(this, false)

      ifdef windows then
        let timers = Timers
        let pm: ProcessMonitor tag = this
        let tn =
          object iso is TimerNotify
            fun ref apply(timer: Timer, count: U64): Bool =>
              pm.timer_notify()
              true
          end
        let timer = Timer(consume tn, 10_000_000, 100_000_000)
        timers(consume timer)
        _timers = timers
      end
    else
      _notifier.failed(this, ForkError)
      return
    end
    _notifier.created(this)

  be print(data: ByteSeq) =>
    """
    Print some bytes and append a newline.
    """
    if not _done_writing then
      _stdin_writeable = _write_final(data)
      _stdin_writeable = _write_final("\n")
    end

  be write(data: ByteSeq) =>
    """
    Write to STDIN of the child process.
    """
    if not _done_writing then
      _stdin_writeable = _write_final(data)
    end

  be printv(data: ByteSeqIter) =>
    """
    Print an iterable collection of ByteSeqs.
    """
    for bytes in data.values() do
      _stdin_writeable = _write_final(bytes)
      _stdin_writeable = _write_final("\n")
    end

  be writev(data: ByteSeqIter) =>
    """
    Write an iterable collection of ByteSeqs.
    """
    for bytes in data.values() do
      _stdin_writeable = _write_final(bytes)
    end

  be done_writing() =>
    """
    Set the _done_writing flag to true. If the _pending is empty we can
    close the _stdin_write file descriptor.
    """
    Debug("done_writing")
    _done_writing = true
    Backpressure.release(_backpressure_auth)
    if _pending.size() == 0 then
      _stdin.close_near()
    end

  be dispose() =>
    """
    Terminate child and close down everything.
    """
    Backpressure.release(_backpressure_auth)
    _child.kill()
    _close()

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
        Debug("event: stdin(" + event.usize().string() + ") Writable")
        _stdin_writeable = _pending_writes()
      elseif AsioEvent.disposable(flags) then
        Debug("event: stdin(" + event.usize().string() + ") Disposable")
        _stdin.dispose()
      else
        Debug("event: stdin(" + event.usize().string() + ") " + flags.string())
      end
    | _stdout.event =>
      if AsioEvent.readable(flags) then
        Debug("event: stdout(" + event.usize().string() + ") Readable")
        _stdout_readable = _pending_reads(_stdout)
      elseif AsioEvent.disposable(flags) then
        Debug("event: stdout(" + event.usize().string() + ") Disposable")
        _stdout.dispose()
      else
        Debug("event: stdout(" + event.usize().string() + ") " + flags.string())
      end
    | _stderr.event =>
      if AsioEvent.readable(flags) then
        Debug("event: stderr(" + event.usize().string() + ") Readable")
        _stderr_readable = _pending_reads(_stderr)
      elseif AsioEvent.disposable(flags) then
        Debug("event: stderr(" + event.usize().string() + ") Disposable")
        _stderr.dispose()
      else
        Debug("event: stderr(" + event.usize().string() + ") " + flags.string())
      end
    end
    _try_shutdown()

  be timer_notify() =>
    """
    Windows IO polling timer has fired
    """
    Debug("timer_notify:: stdin")
    _stdin_writeable = _pending_writes() // try writes
    Debug("timer_notify:: stdout")
    _stdout_readable = _pending_reads(_stdout)
    Debug("timer_notify:: stderr")
    _stderr_readable = _pending_reads(_stderr)
    _try_shutdown()

  fun ref _close() =>
    """
    Close all pipes and wait for the child process to exit.
    """
    if not _closed then
      Debug("_close::")
      _closed = true
      _stdin.close()
      _stdout.close()
      _stderr.close()
      _stdin_writeable = false
      _stdout_readable = false
      _stderr_readable = false
      let exit_code = _child.wait()
      if exit_code < 0 then
        // An error waiting for pid
        _notifier.failed(this, WaitpidError)
      else
        // process child exit code
        _notifier.dispose(this, exit_code)
      end
      match _timers
      | let t: Timers => t.dispose()
      end
    end

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

  fun ref _pending_reads(pipe: _Pipe): Bool =>
    """
    Read from stdout or stderr while data is available. If we read 4 kb of
    data, send ourself a resume message and stop reading, to avoid starving
    other actors.
    It's safe to use the same buffer for stdout and stderr because of
    causal messaging. Events get processed one _after_ another.
    """
    Debug("_pending_reads:: near_fd:" + pipe.near_fd.string() + " _closed:" + _closed.string() + " pipe.closed:" + pipe.is_closed().string() + " _read_len:" + _read_len.string())
    if pipe.is_closed() then return false end
    var sum: USize = 0
    while true do
      (_read_buf, let len, let errno) = pipe.read(_read_buf = recover Array[U8] end, _read_len)

      let next = _read_buf.space()
      Debug("  read done: len:" + len.string() + " errno:" + errno.string() + " next:" + next.string())
      match len
      | -1 =>
        if (errno == _EAGAIN()) then
          Debug("  read -1 & EAGAIN, going to try again later")
          return true // nothing to read right now, try again later
        end
        Debug("  read -1, not EAGAIN, pipe must have errored")
        pipe.close()
        return false
      | 0  =>
        Debug("  read 0, pipe must be closed")
        pipe.close()
        return false
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
      end

      _read_len = 0
      _read_buf_size()

      sum = sum + len.usize()
      if sum > (1 << 12) then
        // If we've read 4 kb, yield and read again later.
        Debug("  read a bunch: going to read again. got:" + sum.string())
        _read_again(pipe.near_fd)
        return true
      end
    end
    true

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
    Debug("_read_again:: near_fd:" + near_fd.string())
    match near_fd
    | _stdout.near_fd => _stdout_readable = _pending_reads(_stdout)
    | _stderr.near_fd => _stderr_readable = _pending_reads(_stderr)
    end

  fun ref _write_final(data: ByteSeq): Bool =>
    """
    Write as much as possible to the fd. Return false if not
    everything was written.
    """
    Debug("_write_final:: bytes:" + data.size().string() + " _closed:" + _closed.string() + " stdin.closed:" + _stdin.is_closed().string() + " stdin_writable:" + _stdin_writeable.string())
    if (not _closed) and not _stdin.is_closed() and _stdin_writeable then
      // Send as much data as possible.
      (let len, let errno) = _stdin.write(data, 0)

      if len == -1 then // write error
        if errno == _EAGAIN() then
          // Resource temporarily unavailable, send data later.
          _pending.push((data, 0))
          Backpressure.apply(_backpressure_auth)
          return false
        else
          // notify caller, close fd and bail out
          _notifier.failed(this, WriteError)
          _stdin.close_near()
          return false
        end
      elseif len.usize() < data.size() then
        // Send any remaining data later.
        _pending.push((data, len.usize()))
        Backpressure.apply(_backpressure_auth)
        return false
      end
      return true
    else
      // Send later, when the pipe is available for writing.
      _pending.push((data, 0))
      Backpressure.apply(_backpressure_auth)
      return false
    end

  fun ref _pending_writes(): Bool =>
    """
    Send pending data. If any data can't be sent, keep it and mark fd as not
    writeable. Once set to false the _stdin_writeable flag can only be cleared
    here by processing the pending writes. If the _done_writing flag is set we
    close the _stdin_write fd once we've processed pending writes.
    """
    Debug("_pending_writes:: _closed:" + _closed.string() + " stdin.closed:" + _stdin.is_closed().string() + " stdin_writable:" + _stdin_writeable.string() + " pending.size:" + _pending.size().string())
    while (not _closed) and not _stdin.is_closed() and (_pending.size() > 0) do
      try
        let node = _pending.head()?
        (let data, let offset) = node()?

        // Write as much data as possible.
        (let len, let errno) = _stdin.write(data, offset)

        if len == -1 then // OS signals write error
          if errno == _EAGAIN() then
            // Resource temporarily unavailable, send data later.
            return false
          else
            // Close fd and bail out.
            return false
          end
        elseif (len.usize() + offset) < data.size() then
          // Send remaining data later.
          node()? = (data, offset + len.usize())
          return false
        else
          // This chunk has been fully sent.
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
        _notifier.failed(this, WriteError)
        return false
      end
    end
    true
