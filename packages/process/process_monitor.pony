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
    _env.out.print(err.string())

  fun ref dispose(process: ProcessMonitor ref, child_exit_status: ProcessExitStatus) =>
    let code: I32 = consume child_exit_code
    match child_exit_status
    | let exited: Exited =>
      _env.out.print("Child exit code: " + exited.exit_code().string())
    | let signaled: Signaled =>
      _env.out.print("Child terminated by signal: " + signaled.signal().string())
    end
```

## Process portability

The ProcessMonitor supports spawning processes on Linux, FreeBSD, OSX and Windows.

## Shutting down ProcessMonitor and external process

When a process is spawned using ProcessMonitor, and it is not necessary to communicate to it any further
using `stdin` and `stdout` or `stderr`, calling [done_writing()](process-ProcessMonitor.md#done_writing)
will close stdin to the child process. Processes expecting input will be notified of an `EOF` on their `stdin`
and can terminate.

If a running program needs to be canceled and the [ProcessMonitor](process-ProcessMonitor.md) should be shut down,
calling [dispose](process-ProcessMonitor.md#dispose) will terminate the child
process and clean up all resources.

Once the child process is detected to be closed, the process exit status is retrieved and
[ProcessNotify.dispose](process-ProcessNotify.md#dispose) is called.

The process exit status can be either an instance of [Exited](process-Exited.md) containing
the process exit code in case the program exited on its own,
or (only on posix systems like linux, osx or bsd) an instance of [Signaled](process-Signaled.md) containing the signal number that terminated the process.

"""
use "backpressure"
use "collections"
use "files"
use "time"


type ProcessMonitorAuth is (AmbientAuth | StartProcessAuth)

actor ProcessMonitor
  """
  Fork+execs / creates a child process and monitors it. Notifies a client about
  STDOUT / STDERR events.
  """
  let _notifier: ProcessNotify
  let _backpressure_auth: BackpressureAuth

  var _stdin: _Pipe = _Pipe.none()
  var _stdout: _Pipe = _Pipe.none()
  var _stderr: _Pipe = _Pipe.none()
  var _err: _Pipe = _Pipe.none()
  var _child: _Process = _ProcessNone

  let _max_size: USize = 4096
  var _read_buf: Array[U8] iso = recover Array[U8] .> undefined(_max_size) end
  var _read_len: USize = 0
  var _expect: USize = 0

  embed _pending: List[(ByteSeq, USize)] = _pending.create()
  var _done_writing: Bool = false

  var _closed: Bool = false
  var _timers: (Timers tag | None) = None
  let _process_poll_interval: U64

  new create(
    auth: ProcessMonitorAuth,
    backpressure_auth: BackpressureAuth,
    notifier: ProcessNotify iso,
    filepath: FilePath,
    args: Array[String] val,
    vars: Array[String] val,
    wdir: (FilePath | None) = None,
    process_poll_interval: U64 = Nanos.from_millis(100))
  =>
    """
    Create infrastructure to communicate with a forked child process and
    register the asio events. Fork child process and notify our user about
    incoming data via the notifier.
    """
    _backpressure_auth = backpressure_auth
    _notifier = consume notifier
    _process_poll_interval = process_poll_interval

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

    try
      ifdef posix then
        _child = _ProcessPosix.create(
          filepath.path, args, vars, wdir, _err, _stdin, _stdout, _stderr)?
      elseif windows then
        let windows_child = _ProcessWindows.create(
          filepath.path, args, vars, wdir, _stdin, _stdout, _stderr)
        _child = windows_child
        // notify about errors
        match windows_child.processError
        | let pe: ProcessError =>
          _notifier.failed(this, pe)
          return
        end
      else
        compile_error "unsupported platform"
      end
      _err.begin(this)
      _stdin.begin(this)
      _stdout.begin(this)
      _stderr.begin(this)
    else
      _notifier.failed(this, ProcessError(ForkError))
      return
    end

    // Asio is not wired up for Windows, so use a timer for now.
    ifdef windows then
      _setup_windows_timers()
    end
    _notifier.created(this)


  be print[E: StringEncoder val = UTF8StringEncoder](data: (String | ByteSeq)) =>
    """
    Print some bytes and append a newline.
    """
    if not _done_writing then
      match data
      | let s: String =>
        _write_final(s.array[E]())
        _write_final("\n".array[E]())
      | let bs: ByteSeq =>
        _write_final(bs)
        _write_final("\n".array[E]())
      end
    end

  be write[E: StringEncoder val = UTF8StringEncoder](data: (String | ByteSeq)) =>
    """
    Write to STDIN of the child process.
    """
    if not _done_writing then
      match data
      | let s: String =>
        _write_final(s.array[E]())
      | let bs: ByteSeq =>
        _write_final(bs)
      end
    end

  be printv[E: StringEncoder val = UTF8StringEncoder](data: (StringIter | ByteSeqIter)) =>
    """
    Print an iterable collection of ByteSeqs.
    """
    match data
    | let si: StringIter =>
      for s in si.values() do
        _write_final(s.array[E]())
        _write_final("\n".array[E]())
      end
    | let bsi: ByteSeqIter =>
      for bytes in bsi.values() do
        _write_final(bytes)
        _write_final("\n".array[E]())
      end
    end

  be writev[E: StringEncoder val = UTF8StringEncoder](data: (StringIter | ByteSeqIter)) =>
    """
    Write an iterable collection of ByteSeqs.
    """
    match data
    | let si: StringIter =>
      for s in si.values() do
        _write_final(s.array[E]())
      end
    | let bsi: ByteSeqIter =>
      for bytes in bsi.values() do
        _write_final(bytes)
      end
    end

  be done_writing() =>
    """
    Set the _done_writing flag to true. If _pending is empty we can close the
    _stdin pipe.
    """
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
        _pending_writes()
      elseif AsioEvent.disposable(flags) then
        _stdin.dispose()
      end
    | _stdout.event =>
      if AsioEvent.readable(flags) then
        _pending_reads(_stdout)
      elseif AsioEvent.disposable(flags) then
        _stdout.dispose()
      end
    | _stderr.event =>
      if AsioEvent.readable(flags) then
        _pending_reads(_stderr)
      elseif AsioEvent.disposable(flags) then
        _stderr.dispose()
      end
    | _err.event =>
      if AsioEvent.readable(flags) then
        _pending_reads(_err)
      elseif AsioEvent.disposable(flags) then
        _err.dispose()
      end
    end
    _try_shutdown()

  be timer_notify() =>
    """
    Windows IO polling timer has fired
    """
    _pending_writes() // try writes
    _pending_reads(_stdout)
    _pending_reads(_stderr)
    _try_shutdown()

  fun ref _close() =>
    """
    Close all pipes and wait for the child process to exit.
    """
    if not _closed then
      _closed = true
      _stdin.close()
      _stdout.close()
      _stderr.close()
      _wait_for_child()
    end

  be _wait_for_child() =>
    match _child.wait()
    | _StillRunning =>
      let timers = _ensure_timers()
      let pm: ProcessMonitor tag = this
      let tn =
        object iso is TimerNotify
          fun ref apply(timer: Timer, count: U64): Bool =>
            pm._wait_for_child()
            true
        end
      let timer = Timer(consume tn, _process_poll_interval, _process_poll_interval)
      timers(consume timer)
    | let exit_status: ProcessExitStatus =>
      // process child exit code or termination signal
      _notifier.dispose(this, exit_status)
      _dispose_timers()
    | let wpe: WaitpidError =>
      _notifier.failed(this, ProcessError(WaitpidError))
      _dispose_timers()
    end

  fun ref _ensure_timers(): Timers tag =>
    match _timers
    | None =>
      let ts = Timers
      _timers = ts
      ts
    | let ts: Timers => ts
    end

  fun ref _dispose_timers() =>
    match _timers
    | let ts: Timers =>
      ts.dispose()
      _timers = None
    end

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

  fun ref _pending_reads(pipe: _Pipe) =>
    """
    Read from stdout or stderr while data is available. If we read 4 kb of
    data, send ourself a resume message and stop reading, to avoid starving
    other actors.
    It's safe to use the same buffer for stdout and stderr because of
    causal messaging. Events get processed one _after_ another.
    """
    if pipe.is_closed() then return end
    var sum: USize = 0
    while true do
      (_read_buf, let len, let errno) =
        pipe.read(_read_buf = recover Array[U8] end, _read_len)

      let next = _read_buf.space()
      match len
      | -1 =>
        if (errno == _EAGAIN()) then
          return // nothing to read right now, try again later
        end
        pipe.close()
        return
      | 0  =>
        pipe.close()
        return
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
      | _err.near_fd =>
        let step: U8 = try data.read_u8(0)? else -1 end
        match step
        | _StepChdir() =>
          _notifier.failed(this, ProcessError(ChdirError))
        | _StepExecve() =>
          _notifier.failed(this, ProcessError(ExecveError))
        else
          _notifier.failed(this, ProcessError(UnknownError))
        end
      end

      _read_len = 0
      _read_buf_size()

      sum = sum + len.usize()
      if sum > (1 << 12) then
        // If we've read 4 kb, yield and read again later.
        _read_again(pipe.near_fd)
        return
      end
    end

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
    match near_fd
    | _stdout.near_fd => _pending_reads(_stdout)
    | _stderr.near_fd => _pending_reads(_stderr)
    end

  fun ref _write_final(data: ByteSeq) =>
    """
    Write as much as possible to the pipe if it is open and there are no
    pending writes. Save everything unwritten into _pending and apply
    backpressure.
    """
    if (not _closed) and not _stdin.is_closed() and (_pending.size() == 0) then
      // Send as much data as possible.
      (let len, let errno) = _stdin.write(data, 0)

      if len == -1 then // write error
        if errno == _EAGAIN() then
          // Resource temporarily unavailable, send data later.
          _pending.push((data, 0))
          Backpressure.apply(_backpressure_auth)
        else
          // Notify caller of error, close fd and done.
          _notifier.failed(this, ProcessError(WriteError))
          _stdin.close_near()
        end
      elseif len.usize() < data.size() then
        // Send any remaining data later.
        _pending.push((data, len.usize()))
        Backpressure.apply(_backpressure_auth)
      end
    else
      // Send later, when the pipe is available for writing.
      _pending.push((data, 0))
      Backpressure.apply(_backpressure_auth)
    end

  fun ref _pending_writes() =>
    """
    Send any pending data. If any data can't be sent, keep it in _pending.
    Once _pending is non-empty, direct writes will get queued there,
    and they can only be written here. If the _done_writing flag is set, close
    the pipe once we've processed pending writes.
    """
    while (not _closed) and not _stdin.is_closed() and (_pending.size() > 0) do
      try
        let node = _pending.head()?
        (let data, let offset) = node()?

        // Write as much data as possible.
        (let len, let errno) = _stdin.write(data, offset)

        if len == -1 then // OS signals write error
          if errno == _EAGAIN() then
            // Resource temporarily unavailable, send data later.
            return
          else
            // Close pipe and bail out.
            _notifier.failed(this, ProcessError(WriteError))
            _stdin.close_near()
            return
          end
        elseif (len.usize() + offset) < data.size() then
          // Send remaining data later.
          node()? = (data, offset + len.usize())
          return
        else
          // This pending chunk has been fully sent.
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
        _notifier.failed(this, ProcessError(WriteError))
        return
      end
    end
