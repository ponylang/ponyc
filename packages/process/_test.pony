use "ponytest"
use "backpressure"
use "capsicum"
use "collections"
use "files"
use "time"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestStdinStdout)
    test(_TestStderr)
    test(_TestFileExecCapabilityIsRequired)
    test(_TestNonExecutablePathResultsInExecveError)
    test(_TestExpect)
    test(_TestWritevOrdering)
    test(_TestPrintvOrdering)
    test(_TestStdinWriteBuf)

class iso _TestStdinStdout is UnitTest
  fun name(): String =>
    "process/STDIN-STDOUT"

  fun exclusion_group(): String =>
    "process-monitor"

  fun apply(h: TestHelper) =>
    let size: USize = 15 // length of "one, two, three" string
    let notifier: ProcessNotify iso = _ProcessClient(size, "", 0, h)
    try
      let path = FilePath(h.env.root as AmbientAuth, "/bin/cat")?
      let args: Array[String] iso = recover Array[String](1) end
      args.push("cat")
      let vars: Array[String] iso = recover Array[String](2) end
      vars.push("HOME=/")
      vars.push("PATH=/bin")

      let auth = h.env.root as AmbientAuth
      let pm: ProcessMonitor =
        ProcessMonitor(auth, auth, consume notifier, path,
          consume args, consume vars)
      pm.write("one, two, three")
      pm.done_writing()  // closing stdin allows "cat" to terminate
      h.dispose_when_done(pm)
      h.long_test(30_000_000_000)
    else
      h.fail("Could not create FilePath!")
    end

  fun timed_out(h: TestHelper) =>
    h.complete(false)

class iso _TestStderr is UnitTest
  var _pm: (ProcessMonitor | None) = None

  fun name(): String =>
    "process/STDERR"

  fun exclusion_group(): String =>
    "process-monitor"

  fun ref apply(h: TestHelper) =>
    let notifier: ProcessNotify iso = _ProcessClient(0,
      "cat: file_does_not_exist: No such file or directory\n", 1, h)
    try
      let path = FilePath(h.env.root as AmbientAuth, "/bin/cat")?
      let args: Array[String] iso = recover Array[String](2) end
      args.push("cat")
      args.push("file_does_not_exist")

      let vars: Array[String] iso = recover Array[String](2) end
      vars.push("HOME=/")
      vars.push("PATH=/bin")

      let auth = h.env.root as AmbientAuth
      _pm  = ProcessMonitor(auth, auth, consume notifier, path,
          consume args, consume vars)
      if _pm isnt None then // write to STDIN of the child process
        let pm = _pm as ProcessMonitor
        pm.done_writing() // closing stdin
        h.dispose_when_done(pm)
      end
      h.long_test(30_000_000_000)
    else
      h.fail("Could not create FilePath!")
    end

  fun timed_out(h: TestHelper) =>
    try
      if _pm isnt None then // kill the child process and cleanup fd
        (_pm as ProcessMonitor).dispose()
      end
    else
      h.fail("Error disposing of forked process in STDIN-WriteBuf test")
    end
    h.complete(false)

class iso _TestFileExecCapabilityIsRequired is UnitTest
  fun name(): String =>
    "process/TestFileExecCapabilityIsRequired"

  fun exclusion_group(): String =>
    "process-monitor"

  fun apply(h: TestHelper) =>
    let notifier: ProcessNotify iso = _ProcessClient(0, "", 1, h)
    try
      let path =
        FilePath(h.env.root as AmbientAuth, "/bin/date",
          recover val FileCaps .> all() .> unset(FileExec) end)?
      let args: Array[String] iso = recover Array[String](1) end
      args.push("date")
      let vars: Array[String] iso = recover Array[String](2) end
      vars.push("HOME=/")
      vars.push("PATH=/bin")

      let auth = h.env.root as AmbientAuth
      let pm: ProcessMonitor =
        ProcessMonitor(auth, auth, consume notifier, path,
          consume args, consume vars)
      h.dispose_when_done(pm)
      h.long_test(30_000_000_000)
    else
      h.fail("Could not create FilePath!")
    end

  fun timed_out(h: TestHelper) =>
    h.complete(false)

class iso _TestNonExecutablePathResultsInExecveError is UnitTest
  fun name(): String =>
    "process/_TestNonExecutablePathResultsInExecveError"

  fun exclusion_group(): String =>
    "process-monitor"

  fun apply(h: TestHelper) =>
    try
      let path = _setup_file(h)?
      let args: Array[String] iso = recover Array[String](1) end
      let vars: Array[String] iso = recover Array[String](2) end

      let auth = h.env.root as AmbientAuth
      let notifier = _setup_notifier(h, path)
      let pm: ProcessMonitor = ProcessMonitor(auth, auth, consume notifier,
        path, consume args, consume vars)
      h.dispose_when_done(pm)
      h.long_test(30_000_000_000)
    else
      h.fail("Could not create FilePath!")
    end

  fun timed_out(h: TestHelper) =>
    h.complete(false)

  fun _setup_notifier(h: TestHelper, path: FilePath): ProcessNotify iso^ =>
    recover object is ProcessNotify
      let _h: TestHelper = h
      let _path: FilePath = path

     fun ref failed(process: ProcessMonitor ref, err: ProcessError) =>
        match err
        | ExecveError => _h.complete(true)
        else
          _h.fail("Unexpected process error")
          _h.complete(false)
        end
        _cleanup()

      fun ref dispose(process: ProcessMonitor ref, child_exit_code: I32) =>
        _h.fail("Dispose shouldn't have been called.")
        _h.complete(false)
        _cleanup()

      fun _cleanup() =>
        _path.remove()
    end end

  fun _setup_file(h: TestHelper): FilePath ? =>
    let tmp_dir = FilePath(h.env.root as AmbientAuth, "/tmp/")?
    let path =
      FilePath(h.env.root as AmbientAuth, tmp_dir.path + "/" + Path.random(32))?
    let tmp_file = CreateFile(path) as File
    let mode = FileMode
    mode.any_exec = false
    tmp_file.path.chmod(consume mode)
    path

class iso _TestExpect is UnitTest
  fun name(): String =>
    "process/Expect"

  fun exclusion_group(): String =>
    "process-monitor"

  fun apply(h: TestHelper) =>
    let notifier =
      recover object is ProcessNotify
        let _h: TestHelper = h
        var _expect: USize = 0
        let _out: Array[String] = Array[String]

        fun ref created(process: ProcessMonitor ref) =>
          process.expect(2)

        fun ref expect(process: ProcessMonitor ref, qty: USize): USize =>
          _expect = qty
          _expect

        fun ref stdout(process: ProcessMonitor ref, data: Array[U8] iso) =>
          _h.assert_eq[USize](_expect, data.size())
          process.expect(if _expect == 2 then 4 else 2 end)
          _out.push(String.from_array(consume data))

        fun ref dispose(process: ProcessMonitor ref, child_exit_code: I32) =>
          _h.assert_eq[I32](child_exit_code, 0)
          _h.assert_array_eq[String](_out, ["he"; "llo "; "th"; "ere!"])
          _h.complete(true)
      end
    end

    try
      let path = FilePath(h.env.root as AmbientAuth, "/bin/echo")?
      let args: Array[String] iso = recover Array[String](1) end
      args.push("echo")
      args.push("hello there!")
      let vars: Array[String] iso = recover Array[String](2) end
      vars.push("HOME=/")
      vars.push("PATH=/bin")

      let auth = h.env.root as AmbientAuth
      let pm: ProcessMonitor = ProcessMonitor(auth, auth, consume notifier,
        path, consume args, consume vars)
      pm.done_writing()  // closing stdin allows "echo" to terminate
      h.dispose_when_done(pm)
      h.long_test(30_000_000_000)
    else
      h.fail("Could not create FilePath!")
    end

  fun timed_out(h: TestHelper) =>
    h.complete(false)

class iso _TestWritevOrdering is UnitTest
  fun name(): String =>
    "process/WritevOrdering"

  fun exclusion_group(): String =>
    "process-monitor"

  fun apply(h: TestHelper) =>
    let notifier: ProcessNotify iso =
      _ProcessClient(11, "", 0, h)
    try
      let path = FilePath(h.env.root as AmbientAuth, "/bin/cat")?
      let args: Array[String] iso = recover Array[String](1) end
      args.push("cat")
      let vars: Array[String] iso = recover Array[String](2) end
      vars.push("HOME=/")
      vars.push("PATH=/bin")

      let auth = h.env.root as AmbientAuth
      let pm: ProcessMonitor =
        ProcessMonitor(auth, auth, consume notifier, path,
          consume args, consume vars)
      let params: Array[String] iso = recover Array[String](3) end
      params.push("one")
      params.push("two")
      params.push("three")

      pm.writev(consume params)
      pm.done_writing()  // closing stdin allows "cat" to terminate
      h.dispose_when_done(pm)
      h.long_test(30_000_000_000)
    else
      h.fail("Could not create FilePath!")
    end

  fun timed_out(h: TestHelper) =>
    h.complete(false)

class iso _TestPrintvOrdering is UnitTest
  fun name(): String =>
    "process/PrintvOrdering"

  fun exclusion_group(): String =>
    "process-monitor"

  fun apply(h: TestHelper) =>
    let notifier: ProcessNotify iso =
      _ProcessClient(14, "", 0, h)
    try
      let path = FilePath(h.env.root as AmbientAuth, "/bin/cat")?
      let args: Array[String] iso = recover Array[String](1) end
      args.push("cat")
      let vars: Array[String] iso = recover Array[String](2) end
      vars.push("HOME=/")
      vars.push("PATH=/bin")

      let auth = h.env.root as AmbientAuth
      let pm: ProcessMonitor =
        ProcessMonitor(auth, auth, consume notifier, path,
          consume args, consume vars)
      let params: Array[String] iso = recover Array[String](3) end
      params.push("one")
      params.push("two")
      params.push("three")

      pm.printv(consume params)
      pm.done_writing()  // closing stdin allows "cat" to terminate
      h.dispose_when_done(pm)
      h.long_test(30_000_000_000)
    else
      h.fail("Could not create FilePath!")
    end

  fun timed_out(h: TestHelper) =>
    h.complete(false)

class iso _TestStdinWriteBuf is UnitTest
  var _pm: (ProcessMonitor | None) = None
  let _test_start: U64 = Time.nanos()

  fun name(): String =>
    "process/STDIN-WriteBuf"

  fun exclusion_group(): String =>
    "process-monitor"

  fun ref apply(h: TestHelper) =>
    let pipe_cap: USize = 65536
    let notifier: ProcessNotify iso = _ProcessClient((pipe_cap + 1) * 2,
      "", 0, h)
    try
      let path = FilePath(h.env.root as AmbientAuth, "/bin/cat")?
      let args: Array[String] iso = recover Array[String](1) end
      args.push("cat")
      let vars: Array[String] iso = recover Array[String](2) end
      vars.push("HOME=/")
      vars.push("PATH=/bin")

      // fork the child process and attach a ProcessMonitor
      let auth = h.env.root as AmbientAuth
      _pm = ProcessMonitor(auth, auth, consume notifier, path, consume args,
        consume vars)

      // create a message larger than pipe_cap bytes
      let message: Array[U8] val = recover Array[U8].>undefined(pipe_cap + 1) end

      if _pm isnt None then // write to STDIN of the child process
        let pm = _pm as ProcessMonitor
        pm.write(message)
        pm.write(message)
        pm.done_writing() // closing stdin allows "cat" to terminate
        h.dispose_when_done(pm)
      end
      h.long_test(30_000_000_000)
    else
      h.fail("Error running STDIN-WriteBuf test")
    end

  fun timed_out(h: TestHelper) =>
    h.log("_TestStdinWriteBuf.timed_out: ran for " +
     (Time.nanos() - _test_start).string() + " ns")
    try
      if _pm isnt None then // kill the child process and cleanup fd
        h.log("_TestStdinWriteBuf.timed_out: calling pm.dispose()")
        (_pm as ProcessMonitor).dispose()
      end
    else
      h.fail("Error disposing of forked process in STDIN-WriteBuf test")
    end
    h.complete(false)

class _ProcessClient is ProcessNotify
  """
  Notifications for Process connections.
  """
  let _out: USize
  let _err: String
  let _exit_code: I32
  let _h: TestHelper
  var _d_stdout_chars: USize = 0
  let _d_stderr: String ref = String
  let _created: U64
  var _first_data: U64 = 0

  new iso create(
    out: USize,
    err: String,
    exit_code: I32,
    h: TestHelper)
  =>
    _out = out
    _err = err
    _exit_code = exit_code
    _h = h
    _created = Time.nanos()

  fun ref stdout(process: ProcessMonitor ref, data: Array[U8] iso) =>
    """
    Called when new data is received on STDOUT of the forked process
    """
    _h.log("\tReceived from stdout: " + data.size().string() + " bytes")
    if (_first_data == 0) then
      _first_data = Time.nanos()
    end
    _d_stdout_chars = _d_stdout_chars + (consume data).size()
    _h.log("\tReceived so far: " + _d_stdout_chars.string() + " bytes")
    _h.log("\tExpecting: " + _out.string() + " bytes")
    if _out == _d_stdout_chars then
      // we've received our total data
      _h.complete(true)
    end

  fun ref stderr(process: ProcessMonitor ref, data: Array[U8] iso) =>
    """
    Called when new data is received on STDERR of the forked process
    """
    _d_stderr.append(consume data)

  fun ref failed(process: ProcessMonitor ref, err: ProcessError) =>
    """
    ProcessMonitor calls this if we run into errors with the
    forked process.
    """
    match err
    | ExecveError => _h.fail("ProcessError: ExecveError")
    | PipeError => _h.fail("ProcessError: PipeError")
    | ForkError => _h.fail("ProcessError: ForkError")
    | WaitpidError => _h.fail("ProcessError: WaitpidError")
    | WriteError => _h.fail("ProcessError: WriteError")
    | KillError => _h.fail("ProcessError: KillError")
    | Unsupported => _h.fail("ProcessError: Unsupported")
    | CapError => _h.complete(true) // used in _TestFileExec
    end

  fun ref dispose(process: ProcessMonitor ref, child_exit_code: I32) =>
    """
    Called when ProcessMonitor terminates to cleanup ProcessNotify
    We receive the exit code of the child process from ProcessMonitor.
    """
    let last_data: U64 = Time.nanos()
    _h.log("dispose: child exit code: " + child_exit_code.string())
    _h.log("dispose: stdout: " + _d_stdout_chars.string() + " bytes")
    _h.log("dispose: stderr: " + _d_stderr)
    if (_first_data > 0) then
      _h.log("dispose: received first data after: \t" + (_first_data - _created).string()
        + " ns")
    end
    _h.log("dispose: total data process_time: \t" + (last_data - _first_data).string()
      + " ns")
    _h.log("dispose: ProcessNotify lifetime: \t" + (last_data - _created).string()
      + " ns")

    _h.assert_eq[USize](_out, _d_stdout_chars)
    _h.assert_eq[String box](_err, _d_stderr)
    _h.assert_eq[I32](_exit_code, child_exit_code)
    _h.complete(true)


