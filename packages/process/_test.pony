use "ponytest"
use "backpressure"
use "capsicum"
use "collections"
use "files"
use "itertools"
use "time"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestFileExecCapabilityIsRequired)
    test(_TestNonExecutablePathResultsInExecveError)
    test(_TestStdinStdout)
    test(_TestStderr)
    test(_TestExpect)
    test(_TestWritevOrdering)
    test(_TestPrintvOrdering)
    test(_TestStdinWriteBuf)

primitive CatPath
  fun apply(): String =>
    ifdef windows then
      "C:\\Windows\\System32\\find.exe"
    else
      "/bin/cat"
    end

primitive CatArgs
  fun apply(): Array[String] val =>
    ifdef windows then
      ["find"; "/v"; "\"\""]
    else
      ["cat"]
    end

primitive EchoPath
  fun apply(): String =>
    ifdef windows then
      "C:\\Windows\\System32\\cmd.exe" // Have to use "cmd /c echo ..."
    else
      "/bin/echo"
    end

class iso _TestFileExecCapabilityIsRequired is UnitTest
  fun name(): String =>
    "process/TestFileExecCapabilityIsRequired"

  fun exclusion_group(): String =>
    "process-monitor"

  fun apply(h: TestHelper) =>
    let notifier: ProcessNotify iso = _ProcessClient(0, "", 1, h)
    try
      let path =
        FilePath(h.env.root, CatPath(),
          recover val FileCaps .> all() .> unset(FileExec) end)?
      let args: Array[String] val = ["dontcare"]
      let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

      let auth = h.env.root
      let pm: ProcessMonitor =
        ProcessMonitor(auth, auth, consume notifier, path, args, vars)
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
      let auth = h.env.root
      let path = FilePath.mkdtemp(auth, "pony_execve_test")?
      let args: Array[String] val = []
      let vars: Array[String] val = []

      let notifier = _setup_notifier(h, path)
      let pm: ProcessMonitor = ProcessMonitor(auth, auth, consume notifier,
        path, args, vars)
      h.dispose_when_done(pm)
      h.long_test(30_000_000_000)
    else
      h.fail("Could not create temporary FilePath!")
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

class iso _TestStdinStdout is UnitTest
  fun name(): String =>
    "process/STDIN-STDOUT"

  fun exclusion_group(): String =>
    "process-monitor"

  fun apply(h: TestHelper) =>
    let input = "one, two, three"
    let size: USize = input.size() + ifdef windows then 2 else 0 end
    let notifier: ProcessNotify iso = _ProcessClient(size, "", 0, h)
    try
      let path = FilePath(h.env.root, CatPath())?
      let args: Array[String] val = CatArgs()
      let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

      let auth = h.env.root
      let pm: ProcessMonitor =
        ProcessMonitor(auth, auth, consume notifier, path, args, vars)
      pm.write(input)
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
    let errmsg =
      ifdef windows then
        "FIND: Invalid switch\r\n"
      else
        "cat: file_does_not_exist: No such file or directory\n"
      end
    let exit_code: I32 = ifdef windows then 2 else 1 end
    let notifier: ProcessNotify iso = _ProcessClient(0, errmsg, exit_code, h)
    try
      let path = FilePath(h.env.root, CatPath())?
      let args: Array[String] val = ifdef windows then
        ["find"; "/q"]
      else
        ["cat"; "file_does_not_exist"]
      end
      let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

      let auth = h.env.root
      _pm  = ProcessMonitor(auth, auth, consume notifier, path, args, vars)
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
    h.complete(false)

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
          let expected: Array[String] val = ifdef windows then
            ["he"; "llo "; "ca"; "rl \r"]
          else
            ["he"; "llo "; "th"; "ere!"]
          end
          _h.assert_array_eq[String](_out, expected)
          _h.complete(true)
      end
    end

    try
      let path = FilePath(h.env.root, EchoPath())?
      let args: Array[String] val = ifdef windows then
        ["cmd"; "/c"; "echo"; "hello carl"]
      else
        ["echo"; "hello there!"]
      end
      let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

      let auth = h.env.root
      let pm: ProcessMonitor = ProcessMonitor(auth, auth, consume notifier,
        path, args, vars)
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
    let expected: USize = ifdef windows then 13 else 11 end
    let notifier: ProcessNotify iso = _ProcessClient(expected, "", 0, h)
    try
      let path = FilePath(h.env.root, CatPath())?
      let args: Array[String] val = CatArgs()
      let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

      let auth = h.env.root
      let pm: ProcessMonitor =
        ProcessMonitor(auth, auth, consume notifier, path, args, vars)
      let params: Array[String] val = ["one"; "two"; "three"]

      pm.writev(params)
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
    let expected: USize = ifdef windows then 17 else 14 end
    let notifier: ProcessNotify iso = _ProcessClient(expected, "", 0, h)
    try
      let path = FilePath(h.env.root, CatPath())?
      let args: Array[String] val = CatArgs()
      let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

      let auth = h.env.root
      let pm: ProcessMonitor =
        ProcessMonitor(auth, auth, consume notifier, path, args, vars)
      let params: Array[String] val = ["one"; "two"; "three"]

      pm.printv(params)
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
      let path = FilePath(h.env.root, CatPath())?
      let args: Array[String] val = CatArgs()
      let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

      // fork the child process and attach a ProcessMonitor
      let auth = h.env.root
      _pm = ProcessMonitor(auth, auth, consume notifier, path, args, vars)

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
    _d_stdout_chars = _d_stdout_chars + data.size()
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
    _h.log("\tReceived from stderr: " + data.size().string() + " bytes")
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
    _h.log("dispose: stderr: '" + _d_stderr + "'")
    if (_first_data > 0) then
      _h.log("dispose: received first data after: \t" + (_first_data - _created).string()
        + " ns")
    end
    _h.log("dispose: total data process_time: \t" + (last_data - _first_data).string()
      + " ns")
    _h.log("dispose: ProcessNotify lifetime: \t" + (last_data - _created).string()
      + " ns")

    _h.assert_eq[USize](_out, _d_stdout_chars)
    _h.assert_eq[USize](_err.size(), _d_stderr.size())
    _h.assert_eq[String box](_err, _d_stderr)
    _h.assert_eq[I32](_exit_code, child_exit_code)
    _h.complete(true)
