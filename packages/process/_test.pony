use "pony_test"
use "backpressure"
use "collections"
use "files"
use "itertools"
use "promises"
use "signals"
use "time"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // factory precondition failures (synchronous)
    test(_TestFileExecCapabilityIsRequired)
    test(_TestExecutableNotFound)
    // real-process behaviour
    test(_TestStdinStdout)
    test(_TestStderr)
    test(_TestExpect)
    test(_TestWritevOrdering)
    test(_TestPrintvOrdering)
    test(_TestChdir)
    test(_TestBadChdir)
    test(_TestBadExec)
    test(_TestWindowsEmptyEnvironment)
    test(_TestStdinWriteBuf)
    test(_TestLongRunningChild)
    test(_TestKillLongRunningChild)
    test(_TestWaitingOnClosedProcessTwice)
    // exit detected independent of the child's pipes
    test(_TestGrandchildDoesNotBlockExit)
    test(_TestWindowsGrandchildDoesNotBlockExit)
    test(_TestStdinOpenChildExits)
    test(_TestGrandchildDeliversBufferedOutput)
    test(_TestChildClosesOutputKeepsRunning)
    test(_TestFloodingGrandchildDoesNotStallTeardown)
    test(_TestManyStartsDoNotLeakFds)
    // state-machine invariants via the injectable seam
    test(_TestExitReportsExactlyOnce)
    test(_TestExitSignalRetriesUntilReapable)
    test(_TestRetryWhileDisposing)
    test(_TestExitSignalReapErrorReportsFailed)
    test(_TestDisposeThenExit)
    test(_TestDoubleDispose)
    test(_TestNoKillAfterReap)
    test(_TestWriteAfterExitIsDropped)
    // backpressure release (real process)
    test(_TestBackpressureAppliedThenReleased)

class \nodoc\ _PathResolver
  let _path: Array[FilePath] val

  new create(env_vars: Array[String] val, auth: FileAuth) =>
    let path =
      for env_var in env_vars.values() do
        try
          if env_var.find("PATH=")? == 0 then
            let paths = Path.split_list(env_var.trim(5))
            let size = paths.size()

            break
              recover val
                Iter[String]((consume paths).values())
                  .map[FilePath]({(str_path) => FilePath(auth, str_path)})
                  .collect[Array[FilePath] ref](Array[FilePath](size))
              end
          end
        end
      end
    match path
    | let paths: Array[FilePath] val => _path = paths
    else
      _path = []
    end

  fun resolve(name: String): FilePath ? =>
    for path_candidate in _path.values() do
      try
        let candidate = path_candidate.join(name)?
        if candidate.exists() then
          return candidate
        end
      end
    end
    error

primitive \nodoc\ _CatCommand
  fun path(path_resolver: _PathResolver): String ? =>
    ifdef windows then
      "C:\\Windows\\System32\\find.exe"
    else
      path_resolver.resolve("cat")?.path
    end

  fun args(): Array[String] val =>
    ifdef windows then
      ["find"; "/v"; "\"\""]
    else
      ["cat"]
    end

class \nodoc\ iso _TestFileExecCapabilityIsRequired is UnitTest
  fun name(): String =>
    "process/TestFileExecCapabilityIsRequired"

  fun exclusion_group(): String =>
    "process-monitor"

  fun apply(h: TestHelper) =>
    try
      let process_auth = StartProcessAuth(h.env.root)
      let backpressure_auth = ApplyReleaseBackpressureAuth(h.env.root)
      let file_auth = FileAuth(h.env.root)
      let path_resolver = _PathResolver(h.env.vars, file_auth)
      let path =
        FilePath(
          file_auth,
          _CatCommand.path(path_resolver)?,
          recover val FileCaps .> all() .> unset(FileExec) end)
      let args: Array[String] val = ["dontcare"]
      let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]
      let notifier = _NoStartNotify(h)

      match StartProcess(process_auth, backpressure_auth, consume notifier,
        path, args, vars)
      | let pm: ProcessMonitor =>
        h.fail("StartProcess should have returned an error")
        pm.dispose()
      | let err: ProcessError =>
        h.assert_true(err.error_type is CapError)
      end
    else
      h.fail("Could not create FilePath!")
    end

class \nodoc\ iso _TestExecutableNotFound is UnitTest
  fun name(): String =>
    "process/ExecutableNotFound"

  fun exclusion_group(): String =>
    "process-monitor"

  fun apply(h: TestHelper) =>
    try
      let process_auth = StartProcessAuth(h.env.root)
      let backpressure_auth = ApplyReleaseBackpressureAuth(h.env.root)
      let file_auth = FileAuth(h.env.root)
      let path = FilePath.mkdtemp(file_auth, "pony_execve_test")?
      let args: Array[String] val = []
      let vars: Array[String] val = []
      let notifier = _NoStartNotify(h)

      match StartProcess(process_auth, backpressure_auth, consume notifier,
        path, args, vars)
      | let pm: ProcessMonitor =>
        h.fail("StartProcess should have returned an error")
        pm.dispose()
      | let err: ProcessError =>
        h.assert_true(err.error_type is ExecutableNotFound)
      end
      path.remove()
    else
      h.fail("Could not create temporary FilePath!")
    end

class \nodoc\ iso _TestStdinStdout is UnitTest
  fun name(): String =>
    "process/STDIN-STDOUT"

  fun exclusion_group(): String =>
    "process-monitor"

  fun apply(h: TestHelper) =>
    let input = "one, two, three"
    let size: USize = input.size() + ifdef windows then 2 else 0 end
    let notifier: ProcessNotify iso = _ProcessClient(size, "", 0, h)
    try
      let process_auth = StartProcessAuth(h.env.root)
      let backpressure_auth = ApplyReleaseBackpressureAuth(h.env.root)
      let file_auth = FileAuth(h.env.root)

      let path_resolver = _PathResolver(h.env.vars, file_auth)
      let path = FilePath(file_auth, _CatCommand.path(path_resolver)?)
      let args: Array[String] val = _CatCommand.args()
      let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

      match StartProcess(process_auth, backpressure_auth, consume notifier,
        path, args, vars)
      | let pm: ProcessMonitor =>
        pm.write(input)
        pm.done_writing() // closing stdin allows "cat" to terminate
        h.dispose_when_done(pm)
      | let err: ProcessError =>
        h.fail("StartProcess failed: " + err.string())
      end
      h.long_test(30_000_000_000)
    else
      h.fail("Could not create FilePath!")
    end

class \nodoc\ iso _TestLongRunningChild is UnitTest
  fun name(): String => "process/long-running-child"
  fun exclusion_group(): String => "process-monitor"
  fun apply(h: TestHelper) =>
    let notifier =
      object iso is ProcessNotify
        fun ref failed(process: ProcessMonitor ref, err: ProcessError) =>
          h.fail(err.string())

        fun ref dispose(process: ProcessMonitor ref,
          child_exit_status: ProcessExitStatus)
        =>
          match child_exit_status
          | Exited(0) => h.complete(true)
          else
            h.fail("process exited with " + child_exit_status.string())
          end
      end

    let process_auth = StartProcessAuth(h.env.root)
    let backpressure_auth = ApplyReleaseBackpressureAuth(h.env.root)
    let file_auth = FileAuth(h.env.root)

    let path = FilePath(file_auth, _SleepPath())
    let args = _SleepArgs(2)
    let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

    match StartProcess(process_auth, backpressure_auth, consume notifier,
      path, args, vars)
    | let pm: ProcessMonitor =>
      pm.done_writing()
      h.dispose_when_done(pm)
    | let err: ProcessError =>
      h.fail("StartProcess failed: " + err.string())
    end
    h.long_test(10_000_000_000)

class \nodoc\ iso _TestKillLongRunningChild is UnitTest
  fun name(): String => "process/kill-long-running-child"
  fun exclusion_group(): String => "process-monitor"
  fun apply(h: TestHelper) =>
    let notifier =
      object iso is ProcessNotify
        fun ref failed(process: ProcessMonitor ref, err: ProcessError) =>
          h.fail(err.string())

        fun ref dispose(process: ProcessMonitor ref,
          child_exit_status: ProcessExitStatus)
        =>
          match child_exit_status
          | Signaled(Sig.term()) =>
            ifdef posix then h.complete(true)
            else h.fail("should not happen on windows.") end
          | Exited(0) =>
            ifdef windows then h.complete(true)
            else h.fail("should be signaled on posix.") end
          else
            h.fail("process exited with " + child_exit_status.string())
            h.complete(false)
          end
      end

    let process_auth = StartProcessAuth(h.env.root)
    let backpressure_auth = ApplyReleaseBackpressureAuth(h.env.root)
    let file_auth = FileAuth(h.env.root)

    let path = FilePath(file_auth, _SleepPath())
    let args = _SleepArgs(2)
    let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

    match StartProcess(process_auth, backpressure_auth, consume notifier,
      path, args, vars)
    | let pm: ProcessMonitor =>
      pm.dispose()
      pm.done_writing()
    | let err: ProcessError =>
      h.fail("StartProcess failed: " + err.string())
    end
    h.long_test(10_000_000_000)

class \nodoc\ iso _TestGrandchildDoesNotBlockExit is UnitTest
  """
  #5764: the child backgrounds a long-lived grandchild that inherits
  stdout/stderr, then exits. The pipes stay open for the grandchild's life,
  but the exit must be reported promptly from the exit event. The timeout is
  well under the grandchild's life, so a pass proves exit was detected without
  waiting on the pipes closing.
  """
  fun name(): String => "process/grandchild-does-not-block-exit"
  fun exclusion_group(): String => "process-monitor"
  fun apply(h: TestHelper) =>
    ifdef windows then
      // posix-only shell semantics; covered on posix.
      h.complete(true)
    else
      let notifier =
        object iso is ProcessNotify
          fun ref failed(process: ProcessMonitor ref, err: ProcessError) =>
            h.fail("failed: " + err.string())
            h.complete(false)

          fun ref dispose(process: ProcessMonitor ref,
            child_exit_status: ProcessExitStatus)
          =>
            match child_exit_status
            | Exited(7) => h.complete(true)
            else
              h.fail("expected Exited(7), got " + child_exit_status.string())
              h.complete(false)
            end
        end

      let process_auth = StartProcessAuth(h.env.root)
      let backpressure_auth = ApplyReleaseBackpressureAuth(h.env.root)
      let file_auth = FileAuth(h.env.root)
      let path = FilePath(file_auth, "/bin/sh")
      let args: Array[String] val = ["sh"; "-c"; "sleep 30 & exit 7"]
      let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

      match StartProcess(process_auth, backpressure_auth, consume notifier,
        path, args, vars)
      | let pm: ProcessMonitor =>
        pm.done_writing()
        h.dispose_when_done(pm)
      | let err: ProcessError =>
        h.fail("StartProcess failed: " + err.string())
        h.complete(false)
      end
      // 10s is well under the grandchild's 30s life.
      h.long_test(10_000_000_000)
    end

class \nodoc\ iso _TestWindowsGrandchildDoesNotBlockExit is UnitTest
  """
  #5764 on Windows. cmd launches a long-lived grandchild (ping) with `start /b`,
  so ping inherits and holds cmd's stdout pipe, then cmd exits 7. The stdout
  pipe stays open for ping's life, but the exit must be reported promptly from
  the process-exit event. The 10s timeout is well under ping's ~30s life, so a
  pass proves exit was detected from the OS, not from the stdout pipe closing.
  """
  fun name(): String => "process/windows-grandchild-does-not-block-exit"
  fun exclusion_group(): String => "process-monitor"
  fun apply(h: TestHelper) =>
    ifdef windows then
      let notifier =
        object iso is ProcessNotify
          fun ref failed(process: ProcessMonitor ref, err: ProcessError) =>
            h.fail("failed: " + err.string())
            h.complete(false)

          fun ref dispose(process: ProcessMonitor ref,
            child_exit_status: ProcessExitStatus)
          =>
            match child_exit_status
            | Exited(7) => h.complete(true)
            else
              h.fail("expected Exited(7), got " + child_exit_status.string())
              h.complete(false)
            end
        end

      let process_auth = StartProcessAuth(h.env.root)
      let backpressure_auth = ApplyReleaseBackpressureAuth(h.env.root)
      let file_auth = FileAuth(h.env.root)
      let path = FilePath(file_auth, "C:\\Windows\\System32\\cmd.exe")
      // `start /b` runs ping without a new console, so it inherits cmd's stdout
      // pipe and keeps it open; cmd then exits 7. A populated environment is
      // required: cmd's `start /b` hangs when the environment block is empty.
      let args: Array[String] val =
        ["cmd"; "/c"; "start /b ping -n 30 127.0.0.1 & exit 7"]
      let vars: Array[String] val =
        ["SystemRoot=C:\\Windows"; "PATH=C:\\Windows\\System32"]

      match StartProcess(process_auth, backpressure_auth, consume notifier,
        path, args, vars)
      | let pm: ProcessMonitor =>
        pm.done_writing()
        h.dispose_when_done(pm)
      | let err: ProcessError =>
        h.fail("StartProcess failed: " + err.string())
        h.complete(false)
      end
      h.long_test(10_000_000_000)
    else
      // Windows-only; on posix there is nothing to run here.
      h.complete(true)
    end

class \nodoc\ iso _TestWindowsEmptyEnvironment is UnitTest
  """
  A child starts with an empty environment. The Windows environment block for
  no variables is two null bytes; CreateProcess rejects a lone null.
  """
  fun name(): String => "process/windows-empty-environment"
  fun exclusion_group(): String => "process-monitor"
  fun apply(h: TestHelper) =>
    ifdef windows then
      let notifier =
        object iso is ProcessNotify
          fun ref failed(process: ProcessMonitor ref, err: ProcessError) =>
            h.fail("failed: " + err.string())
            h.complete(false)

          fun ref dispose(process: ProcessMonitor ref,
            child_exit_status: ProcessExitStatus)
          =>
            match child_exit_status
            | Exited(0) => h.complete(true)
            else
              h.fail("expected Exited(0), got " + child_exit_status.string())
              h.complete(false)
            end
        end

      let process_auth = StartProcessAuth(h.env.root)
      let backpressure_auth = ApplyReleaseBackpressureAuth(h.env.root)
      let file_auth = FileAuth(h.env.root)
      let path = FilePath(file_auth, "C:\\Windows\\System32\\cmd.exe")
      let args: Array[String] val = ["cmd"; "/c"; "exit 0"]
      let vars: Array[String] val = []

      match StartProcess(process_auth, backpressure_auth, consume notifier,
        path, args, vars)
      | let pm: ProcessMonitor =>
        pm.done_writing()
        h.dispose_when_done(pm)
      | let err: ProcessError =>
        h.fail("StartProcess failed: " + err.string())
        h.complete(false)
      end
      h.long_test(10_000_000_000)
    else
      // Windows-only: the empty environment block is a Windows CreateProcess
      // concern.
      h.complete(true)
    end

class \nodoc\ iso _TestStdinOpenChildExits is UnitTest
  """
  #5748: the child exits on its own while we still hold its stdin open (we
  never call done_writing). The exit must still be reported. This also
  exercises the fast-exit probe: the child exits before, or racing with, the
  exit event being armed.
  """
  fun name(): String => "process/stdin-open-child-exits"
  fun exclusion_group(): String => "process-monitor"
  fun apply(h: TestHelper) =>
    ifdef windows then
      h.complete(true)
    else
      let notifier =
        object iso is ProcessNotify
          fun ref failed(process: ProcessMonitor ref, err: ProcessError) =>
            h.fail("failed: " + err.string())
            h.complete(false)

          fun ref dispose(process: ProcessMonitor ref,
            child_exit_status: ProcessExitStatus)
          =>
            match child_exit_status
            | Exited(7) => h.complete(true)
            else
              h.fail("expected Exited(7), got " + child_exit_status.string())
              h.complete(false)
            end
        end

      let process_auth = StartProcessAuth(h.env.root)
      let backpressure_auth = ApplyReleaseBackpressureAuth(h.env.root)
      let file_auth = FileAuth(h.env.root)
      let path = FilePath(file_auth, "/bin/sh")
      let args: Array[String] val = ["sh"; "-c"; "exit 7"]
      let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

      match StartProcess(process_auth, backpressure_auth, consume notifier,
        path, args, vars)
      | let pm: ProcessMonitor =>
        // Deliberately do NOT call done_writing(): stdin stays open.
        h.dispose_when_done(pm)
      | let err: ProcessError =>
        h.fail("StartProcess failed: " + err.string())
        h.complete(false)
      end
      h.long_test(10_000_000_000)
    end

class \nodoc\ iso _TestStderr is UnitTest
  fun name(): String => "process/STDERR"
  fun exclusion_group(): String => "process-monitor"

  fun apply(h: TestHelper) =>
    let errmsg =
      ifdef windows then
        "message-to-stderr\r\n"
      else
        "cat: file_does_not_exist: No such file or directory\n"
      end
    let exit_code: I32 = ifdef windows then 0 else 1 end
    let notifier: ProcessNotify iso = _ProcessClient(0, errmsg, exit_code, h)
    try
      let process_auth = StartProcessAuth(h.env.root)
      let backpressure_auth = ApplyReleaseBackpressureAuth(h.env.root)
      let file_auth = FileAuth(h.env.root)

      let path_resolver = _PathResolver(h.env.vars, file_auth)
      let path = FilePath(
        file_auth,
        ifdef windows then
          "C:\\Windows\\System32\\cmd.exe"
        else
          _CatCommand.path(path_resolver)?
        end)
      let args: Array[String] val = ifdef windows then
        ["cmd"; "/c"; "\"(echo message-to-stderr)1>&2\""]
      else
        ["cat"; "file_does_not_exist"]
      end
      let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

      match StartProcess(process_auth, backpressure_auth, consume notifier,
        path, args, vars)
      | let pm: ProcessMonitor =>
        pm.done_writing()
        h.dispose_when_done(pm)
      | let err: ProcessError =>
        h.fail("StartProcess failed: " + err.string())
      end
      h.long_test(30_000_000_000)
    else
      h.fail("Could not create FilePath!")
    end

class \nodoc\ iso _TestExpect is UnitTest
  fun name(): String => "process/Expect"
  fun exclusion_group(): String => "process-monitor"

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

        fun ref dispose(process: ProcessMonitor ref,
          child_exit_status: ProcessExitStatus)
        =>
          match child_exit_status
          | Exited(0) =>
            let expected: Array[String] val = ifdef windows then
              ["he"; "llo "; "ca"; "rl \r"]
            else
              ["he"; "llo "; "th"; "ere!"]
            end
            _h.assert_array_eq[String](_out, expected)
            _h.complete(true)
          else
            _h.fail("child exited with: " + child_exit_status.string())
            _h.complete(false)
          end
      end end

    try
      let process_auth = StartProcessAuth(h.env.root)
      let backpressure_auth = ApplyReleaseBackpressureAuth(h.env.root)
      let file_auth = FileAuth(h.env.root)

      let path_resolver = _PathResolver(h.env.vars, file_auth)
      let path = FilePath(file_auth, _EchoPath(path_resolver)?)
      let args: Array[String] val = ifdef windows then
        ["cmd"; "/c"; "echo"; "hello carl"]
      else
        ["echo"; "hello there!"]
      end
      let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

      match StartProcess(process_auth, backpressure_auth, consume notifier,
        path, args, vars)
      | let pm: ProcessMonitor =>
        pm.done_writing()
        h.dispose_when_done(pm)
      | let err: ProcessError =>
        h.fail("StartProcess failed: " + err.string())
      end
      h.long_test(30_000_000_000)
    else
      h.fail("Could not create FilePath!")
    end

class \nodoc\ iso _TestWritevOrdering is UnitTest
  fun name(): String => "process/WritevOrdering"
  fun exclusion_group(): String => "process-monitor"

  fun apply(h: TestHelper) =>
    let expected: USize = ifdef windows then 13 else 11 end
    let notifier: ProcessNotify iso = _ProcessClient(expected, "", 0, h)
    try
      let process_auth = StartProcessAuth(h.env.root)
      let backpressure_auth = ApplyReleaseBackpressureAuth(h.env.root)
      let file_auth = FileAuth(h.env.root)

      let path_resolver = _PathResolver(h.env.vars, file_auth)
      let path = FilePath(file_auth, _CatCommand.path(path_resolver)?)
      let args: Array[String] val = _CatCommand.args()
      let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

      match StartProcess(process_auth, backpressure_auth, consume notifier,
        path, args, vars)
      | let pm: ProcessMonitor =>
        pm.writev(["one"; "two"; "three"])
        pm.done_writing()
        h.dispose_when_done(pm)
      | let err: ProcessError =>
        h.fail("StartProcess failed: " + err.string())
      end
      h.long_test(30_000_000_000)
    else
      h.fail("Could not create FilePath!")
    end

class \nodoc\ iso _TestPrintvOrdering is UnitTest
  fun name(): String => "process/PrintvOrdering"
  fun exclusion_group(): String => "process-monitor"

  fun apply(h: TestHelper) =>
    let expected: USize = ifdef windows then 17 else 14 end
    let notifier: ProcessNotify iso = _ProcessClient(expected, "", 0, h)
    try
      let process_auth = StartProcessAuth(h.env.root)
      let backpressure_auth = ApplyReleaseBackpressureAuth(h.env.root)
      let file_auth = FileAuth(h.env.root)

      let path_resolver = _PathResolver(h.env.vars, file_auth)
      let path = FilePath(file_auth, _CatCommand.path(path_resolver)?)
      let args: Array[String] val = _CatCommand.args()
      let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

      match StartProcess(process_auth, backpressure_auth, consume notifier,
        path, args, vars)
      | let pm: ProcessMonitor =>
        pm.printv(["one"; "two"; "three"])
        pm.done_writing()
        h.dispose_when_done(pm)
      | let err: ProcessError =>
        h.fail("StartProcess failed: " + err.string())
      end
      h.long_test(30_000_000_000)
    else
      h.fail("Could not create FilePath!")
    end

class \nodoc\ _TestChdir is UnitTest
  fun name(): String => "process/Chdir"
  fun exclusion_group(): String => "process-monitor"

  fun ref apply(h: TestHelper) =>
    let process_auth = StartProcessAuth(h.env.root)
    let backpressure_auth = ApplyReleaseBackpressureAuth(h.env.root)
    let file_auth = FileAuth(h.env.root)

    let parent = Path.dir(Path.cwd())
    let notifier: ProcessNotify iso = _ProcessClient(parent.size()
      + (ifdef windows then 2 else 1 end), "", 0, h)

    let path = FilePath(file_auth, _PwdPath())
    let args: Array[String] val = _PwdArgs()
    let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

    match StartProcess(process_auth, backpressure_auth, consume notifier,
      path, args, vars, FilePath(file_auth, parent))
    | let pm: ProcessMonitor =>
      pm.done_writing()
      h.dispose_when_done(pm)
    | let err: ProcessError =>
      h.fail("StartProcess failed: " + err.string())
    end
    h.long_test(30_000_000_000)

class \nodoc\ _TestBadChdir is UnitTest
  fun name(): String => "process/BadChdir"
  fun exclusion_group(): String => "process-monitor"

  fun ref apply(h: TestHelper) =>
    let badpath = Path.abs(Path.random(10))
    let process_auth = StartProcessAuth(h.env.root)
    let backpressure_auth = ApplyReleaseBackpressureAuth(h.env.root)
    let file_auth = FileAuth(h.env.root)
    let path = FilePath(file_auth, _PwdPath())
    let args: Array[String] val = _PwdArgs()
    let vars: Array[String] val = []
    let bad_wdir = FilePath(file_auth, badpath)

    ifdef windows then
      // Windows CreateProcess fails synchronously for a bad working directory,
      // so StartProcess returns the error rather than reporting it through the
      // notifier; there is no fork/exec split to defer it to.
      let notifier = _NoStartNotify(h)
      match StartProcess(process_auth, backpressure_auth, consume notifier,
        path, args, vars, bad_wdir)
      | let pm: ProcessMonitor =>
        h.fail("StartProcess should have returned an error")
        pm.dispose()
      | let err: ProcessError =>
        h.assert_true(err.error_type is ChdirError)
      end
    else
      let notifier: ProcessNotify iso = _FailAndExitClient(h, ChdirError)
      match StartProcess(process_auth, backpressure_auth, consume notifier,
        path, args, vars, bad_wdir)
      | let pm: ProcessMonitor =>
        pm.done_writing()
        h.dispose_when_done(pm)
      | let err: ProcessError =>
        h.fail("StartProcess failed: " + err.string())
      end
      h.long_test(30_000_000_000)
    end

class \nodoc\ _TestBadExec is UnitTest
  let _bad_exec_sh_contents: String =
    """
    #!/usr/bin/please-dont-exist

    true
    """
  var _tmp_dir: (FilePath | None) = None
  var _bad_exec_path: (FilePath | None) = None

  fun name(): String => "process/BadExec"
  fun exclusion_group(): String => "process-monitor"

  fun ref set_up(h: TestHelper) ? =>
    let file_auth = FileAuth(h.env.root)
    ifdef windows then
      _bad_exec_path = FilePath(file_auth, "C:\\Windows\\system.ini")
    else
      let tmp_dir = FilePath.mkdtemp(file_auth,
        "pony_stdlib_test_process_bad_exec")?
      _tmp_dir = tmp_dir
      let bad_exec_path = tmp_dir.join("_bad_exec.sh")?
      _bad_exec_path = bad_exec_path
      let file = File(bad_exec_path)
      file.write(_bad_exec_sh_contents)
      if not file.chmod(FileMode .> exec()) then error end
    end

  fun ref tear_down(h: TestHelper) =>
    try (_tmp_dir as FilePath).remove() end

  fun ref apply(h: TestHelper) =>
    try
      let process_auth = StartProcessAuth(h.env.root)
      let backpressure_auth = ApplyReleaseBackpressureAuth(h.env.root)
      let path = _bad_exec_path as FilePath

      ifdef windows then
        // Windows CreateProcess fails synchronously on a non-executable file,
        // so StartProcess returns the error rather than reporting it through the
        // notifier.
        let notifier = _NoStartNotify(h)
        match StartProcess(process_auth, backpressure_auth, consume notifier,
          path, [], [])
        | let pm: ProcessMonitor =>
          h.fail("StartProcess should have returned an error")
          pm.dispose()
        | let err: ProcessError =>
          h.assert_true(err.error_type is ExecveError)
        end
      else
        let notifier: ProcessNotify iso = _FailAndExitClient(h, ExecveError)
        match StartProcess(process_auth, backpressure_auth, consume notifier,
          path, [], [])
        | let pm: ProcessMonitor =>
          pm.done_writing()
          h.dispose_when_done(pm)
        | let err: ProcessError =>
          h.fail("StartProcess failed: " + err.string())
        end
        h.long_test(30_000_000_000)
      end
    else
      h.fail("bad_exec_path not set!")
    end

class \nodoc\ iso _TestStdinWriteBuf is UnitTest
  var _pm: (ProcessMonitor | None) = None
  let _test_start: U64 = Time.nanos()

  fun name(): String => "process/STDIN-WriteBuf"
  fun exclusion_group(): String => "process-monitor"

  fun ref apply(h: TestHelper) =>
    let pipe_cap: USize = 65536
    let message: Array[U8] val = recover Array[U8].init('\n', pipe_cap + 1) end
    let out_size = ifdef windows then message.size() * 2 else message.size() end
    let notifier: ProcessNotify iso = _ProcessClient(out_size, "", 0, h)
    try
      let process_auth = StartProcessAuth(h.env.root)
      let backpressure_auth = ApplyReleaseBackpressureAuth(h.env.root)
      let file_auth = FileAuth(h.env.root)

      let path_resolver = _PathResolver(h.env.vars, file_auth)
      let path = FilePath(file_auth, _CatCommand.path(path_resolver)?)
      let args: Array[String] val = _CatCommand.args()
      let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

      match StartProcess(process_auth, backpressure_auth, consume notifier,
        path, args, vars)
      | let pm: ProcessMonitor =>
        _pm = pm
        pm.write(message)
        pm.done_writing()
        h.dispose_when_done(pm)
      | let err: ProcessError =>
        h.fail("StartProcess failed: " + err.string())
      end
      h.long_test(30_000_000_000)
    else
      h.fail("Error running STDIN-WriteBuf test")
    end

  fun timed_out(h: TestHelper) =>
    h.log("_TestStdinWriteBuf.timed_out: ran for " +
      (Time.nanos() - _test_start).string() + " ns")
    try (_pm as ProcessMonitor).dispose() end

class \nodoc\ _TestWaitingOnClosedProcessTwice is UnitTest
  """
  #5765 regression, real process: dispose() twice must report the exit status
  exactly once. We start a short-lived process, let it finish, then send two
  dispose() messages and verify the notifier's dispose fires only once.
  """
  var _timers: (Timers | None) = None

  fun name(): String => "process/wait-on-closed-process-twice"
  fun exclusion_group(): String => "process-monitor"

  fun ref tear_down(h: TestHelper) =>
    try (_timers as Timers).dispose() end

  fun ref apply(h: TestHelper) =>
    let process_auth = StartProcessAuth(h.env.root)
    let backpressure_auth = ApplyReleaseBackpressureAuth(h.env.root)
    let file_auth = FileAuth(h.env.root)

    let path = FilePath(file_auth, _SleepPath())
    let args = _SleepArgs(1)
    let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

    let timers = Timers
    _timers = timers

    let pn = object iso is ProcessNotify
      var n: USize = 0
      fun ref dispose(pm: ProcessMonitor ref, status: ProcessExitStatus) =>
        n = n + 1
        h.log("dispose() called " + n.string() + " time")
        match status
        | Exited(0) => h.log("child exited")
        else
          h.fail("child did not exit cleanly")
          h.complete(false)
          return
        end

        if n == 1 then
          let timer2 = Timer(
            object iso is TimerNotify
              fun ref apply(timer: Timer, count: U64): Bool =>
                if n == 1 then
                  h.complete(true)
                else
                  h.fail("notifier received dispose() more than once")
                  h.complete(false)
                end
                true
            end,
            500_000_000, 0)
          timers(consume timer2)
        end
    end

    match StartProcess(process_auth, backpressure_auth, consume pn, path, args,
      vars)
    | let pm: ProcessMonitor =>
      let timer1 = Timer(
        object iso is TimerNotify
          fun ref apply(timer: Timer, count: U64): Bool =>
            pm.dispose()
            pm.dispose()
            true
        end,
        3_000_000_000, 0)
      timers(consume timer1)
    | let err: ProcessError =>
      h.fail("StartProcess failed: " + err.string())
    end
    h.long_test(10_000_000_000)

primitive \nodoc\ _EchoPath
  fun apply(path_resolver: _PathResolver): String ? =>
    ifdef windows then
      "C:\\Windows\\System32\\cmd.exe"
    else
      path_resolver.resolve("echo")?.path
    end

primitive \nodoc\ _PwdPath
  fun apply(): String =>
    ifdef windows then
      "C:\\Windows\\System32\\cmd.exe"
    else
      "/bin/pwd"
    end

primitive \nodoc\ _PwdArgs
  fun apply(): Array[String] val =>
    ifdef windows then
      ["cmd"; "/c"; "cd"]
    else
      ["pwd"]
    end

primitive \nodoc\ _SleepPath
  fun apply(): String =>
    ifdef windows then
      "C:\\Windows\\System32\\ping.exe"
    else
      "/bin/sleep"
    end

primitive \nodoc\ _SleepArgs
  fun apply(seconds: USize): Array[String] val =>
    ifdef windows then
      ["ping"; "127.0.0.1"; "-n"; seconds.string()]
    else
      ["sleep"; seconds.string()]
    end

class \nodoc\ _NoStartNotify is ProcessNotify
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref stdout(process: ProcessMonitor ref, data: Array[U8] iso) =>
    _h.fail("stdout shouldn't be called")

  fun ref stderr(process: ProcessMonitor ref, data: Array[U8] iso) =>
    _h.fail("stderr shouldn't be called")

  fun ref failed(process: ProcessMonitor ref, actual: ProcessError) =>
    _h.fail("failed shouldn't be called")

  fun ref dispose(process: ProcessMonitor ref,
    child_exit_status: ProcessExitStatus)
  =>
    _h.fail("dispose shouldn't be called")

class \nodoc\ _ProcessClient is ProcessNotify
  let _out: USize
  let _err: String
  let _exit_code: I32
  let _h: TestHelper
  var _d_stdout_chars: USize = 0
  let _d_stderr: String ref = String
  let _expected_err: (ProcessError | None)

  new iso create(
    out: USize,
    err: String,
    exit_code: I32,
    h: TestHelper,
    expected_err: (ProcessError | None) = None)
  =>
    _out = out
    _err = err
    _exit_code = exit_code
    _expected_err = expected_err
    _h = h

  fun ref stdout(process: ProcessMonitor ref, data: Array[U8] iso) =>
    _h.log("\tReceived from stdout: " + data.size().string() + " bytes")
    _d_stdout_chars = _d_stdout_chars + data.size()

  fun ref stderr(process: ProcessMonitor ref, data: Array[U8] iso) =>
    _h.log("\tReceived from stderr: " + data.size().string() + " bytes")
    _d_stderr.append(consume data)

  fun ref failed(process: ProcessMonitor ref, actual: ProcessError) =>
    match _expected_err
    | let expected: ProcessError =>
      if actual.error_type is expected.error_type then
        _h.complete(true)
      else
        _h.fail("Expected '" + expected.string() + "'; actual was '"
          + actual.string() + "'")
      end
    else
      _h.fail(actual.string())
    end

  fun ref dispose(process: ProcessMonitor ref,
    child_exit_status: ProcessExitStatus)
  =>
    match \exhaustive\ child_exit_status
    | let e: Exited =>
      _h.log("dispose: child exit code: " + e.exit_code().string())
      _h.assert_eq[I32](_exit_code, e.exit_code())
    | let s: Signaled =>
      _h.log("dispose: child terminated by signal: " + s.signal().string())
      _h.fail("didn't expect that.")
    end
    _h.assert_eq[USize](_out, _d_stdout_chars)
    _h.assert_eq[USize](_err.size(), _d_stderr.size())
    _h.assert_eq[String box](_err, _d_stderr)
    _h.complete(true)

// --------------------------------------------------------------------------
// The redesign's motivating cases, continued
// --------------------------------------------------------------------------

class \nodoc\ iso _TestGrandchildDeliversBufferedOutput is UnitTest
  """
  The child writes output, then backgrounds a long-lived grandchild that
  inherits stdout, then exits. The child's own output must be delivered before
  `dispose`, and `dispose` must arrive promptly (well under the grandchild's
  life), not wait for the grandchild.
  """
  fun name(): String => "process/grandchild-delivers-buffered-output"
  fun exclusion_group(): String => "process-monitor"
  fun apply(h: TestHelper) =>
    ifdef windows then
      h.complete(true)
    else
      let notifier: ProcessNotify iso = _ProcessClient(6, "", 0, h)
      let process_auth = StartProcessAuth(h.env.root)
      let backpressure_auth = ApplyReleaseBackpressureAuth(h.env.root)
      let file_auth = FileAuth(h.env.root)
      let path = FilePath(file_auth, "/bin/sh")
      let args: Array[String] val =
        ["sh"; "-c"; "echo hello; sleep 30 & exit 0"]
      let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

      match StartProcess(process_auth, backpressure_auth, consume notifier,
        path, args, vars)
      | let pm: ProcessMonitor =>
        pm.done_writing()
        h.dispose_when_done(pm)
      | let err: ProcessError =>
        h.fail("StartProcess failed: " + err.string())
      end
      h.long_test(10_000_000_000)
    end

class \nodoc\ iso _TestChildClosesOutputKeepsRunning is UnitTest
  """
  The child closes its own stdout and stderr, then keeps running and exits on
  its own while we still hold its stdin open. Closing output must not tear the
  monitor down; the exit is reported from the exit event when the child
  actually exits.
  """
  fun name(): String => "process/child-closes-output-keeps-running"
  fun exclusion_group(): String => "process-monitor"
  fun apply(h: TestHelper) =>
    ifdef windows then
      h.complete(true)
    else
      let notifier =
        object iso is ProcessNotify
          fun ref failed(process: ProcessMonitor ref, err: ProcessError) =>
            h.fail("failed: " + err.string())
            h.complete(false)

          fun ref dispose(process: ProcessMonitor ref,
            status: ProcessExitStatus)
          =>
            match status
            | Exited(5) => h.complete(true)
            else
              h.fail("expected Exited(5), got " + status.string())
              h.complete(false)
            end
        end

      let process_auth = StartProcessAuth(h.env.root)
      let backpressure_auth = ApplyReleaseBackpressureAuth(h.env.root)
      let file_auth = FileAuth(h.env.root)
      let path = FilePath(file_auth, "/bin/sh")
      // close stdout+stderr, sleep, exit 5 — all while we hold stdin open.
      let args: Array[String] val =
        ["sh"; "-c"; "exec 1>&- 2>&-; sleep 1; exit 5"]
      let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

      match StartProcess(process_auth, backpressure_auth, consume notifier,
        path, args, vars)
      | let pm: ProcessMonitor =>
        // Deliberately do not call done_writing(): stdin stays open.
        h.dispose_when_done(pm)
      | let err: ProcessError =>
        h.fail("StartProcess failed: " + err.string())
        h.complete(false)
      end
      h.long_test(10_000_000_000)
    end

class \nodoc\ iso _TestBackpressureAppliedThenReleased is UnitTest
  """
  A child that ignores its stdin, written more than a pipe buffer, makes the
  monitor queue the overflow and apply backpressure. Disposing must release it.
  The test asserts backpressure was applied first, so a never-applies
  implementation cannot pass.
  """
  fun name(): String => "process/backpressure-applied-then-released"
  fun exclusion_group(): String => "process-monitor"
  fun apply(h: TestHelper) =>
    ifdef windows then
      h.complete(true)
    else
      let notifier =
        object iso is ProcessNotify
          fun ref dispose(process: ProcessMonitor ref,
            status: ProcessExitStatus)
          =>
            None
        end

      let process_auth = StartProcessAuth(h.env.root)
      let backpressure_auth = ApplyReleaseBackpressureAuth(h.env.root)
      let file_auth = FileAuth(h.env.root)
      let path = FilePath(file_auth, "/bin/sleep")
      let args: Array[String] val = ["sleep"; "5"]
      let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

      // 256 KB, well over a default pipe buffer, so the write queues and
      // applies backpressure.
      let message: Array[U8] val = recover Array[U8].init('x', 262144) end

      match StartProcess(process_auth, backpressure_auth, consume notifier,
        path, args, vars)
      | let pm: ProcessMonitor =>
        pm.write(message)
        let applied_q = Promise[Bool]
        pm._test_query_backpressure(applied_q)
        applied_q.next[None]({(applied)(h, pm) =>
          h.assert_true(applied,
            "backpressure should be applied after an over-buffer write")
          pm.dispose()
          let released_q = Promise[Bool]
          pm._test_query_backpressure(released_q)
          released_q.next[None]({(still)(h) =>
            if still then
              h.fail("backpressure still applied after dispose")
              h.complete(false)
            else
              h.complete(true)
            end
          })
        })
      | let err: ProcessError =>
        h.fail("StartProcess failed: " + err.string())
        h.complete(false)
      end
      h.long_test(10_000_000_000)
    end

// --------------------------------------------------------------------------
// State-machine invariants via the injectable _Process seam
// --------------------------------------------------------------------------

class \nodoc\ val _SpyResults
  let disposes: USize
  let failures: USize
  let kills: USize
  let kill_after_reap: Bool
  let last_status: (ProcessExitStatus | None)
  let backpressure_applied: Bool

  new val create(disposes': USize, failures': USize, kills': USize,
    kill_after_reap': Bool, last_status': (ProcessExitStatus | None),
    backpressure_applied': Bool)
  =>
    disposes = disposes'
    failures = failures'
    kills = kills'
    kill_after_reap = kill_after_reap'
    last_status = last_status'
    backpressure_applied = backpressure_applied'

  fun last_is(s: ProcessExitStatus): Bool =>
    match last_status
    | let x: ProcessExitStatus => x == s
    else
      false
    end

  fun string(): String iso^ =>
    recover
      String
        .> append("disposes=" + disposes.string())
        .> append(" failures=" + failures.string())
        .> append(" kills=" + kills.string())
        .> append(" kill_after_reap=" + kill_after_reap.string())
        .> append(" backpressure=" + backpressure_applied.string())
    end

actor \nodoc\ _SpyRecorder
  var _kills: USize = 0
  var _disposes: USize = 0
  var _failures: USize = 0
  var _reaped: Bool = false
  var _kill_after_reap: Bool = false
  var _last_status: (ProcessExitStatus | None) = None

  be on_kill() =>
    _kills = _kills + 1
    if _reaped then _kill_after_reap = true end

  be on_reap() =>
    _reaped = true

  be on_dispose(s: ProcessExitStatus) =>
    _disposes = _disposes + 1
    _last_status = s

  be on_failed() =>
    _failures = _failures + 1

  be results(backpressure_applied: Bool, p: Promise[_SpyResults]) =>
    p(_SpyResults(_disposes, _failures, _kills, _kill_after_reap, _last_status,
      backpressure_applied))

class \nodoc\ _ProcessSpy is _Process
  """
  A stand-in for a real child. Records `kill()` to a recorder and returns a
  chosen result from `wait()`. The first `wait()` (the constructor probe)
  returns `_StillRunning`; a later `wait()` (a triggered exit) reaps. `lag`
  extends the `_StillRunning` window past the probe by that many `wait()` calls,
  standing in for waitpid trailing the OS exit notification. With `fail`, the
  reap returns `WaitpidError` instead of the status.
  """
  let _recorder: _SpyRecorder
  let _status: ProcessExitStatus
  let _lag: USize
  let _fail: Bool
  var _waits: USize = 0

  new create(recorder: _SpyRecorder, status: ProcessExitStatus,
    lag: USize = 0, fail: Bool = false)
  =>
    _recorder = recorder
    _status = status
    _lag = lag
    _fail = fail

  fun box kill() =>
    _recorder.on_kill()

  fun ref wait(): _WaitResult =>
    _waits = _waits + 1
    if _waits <= (1 + _lag) then
      _StillRunning
    elseif _fail then
      WaitpidError
    else
      _recorder.on_reap()
      _status
    end

  fun ref arm_exit_event(owner: AsioEventNotify): AsioEventID =>
    AsioEvent.none()

  fun ref close_exit_source(event: AsioEventID) =>
    None

class \nodoc\ _SpyNotify is ProcessNotify
  let _recorder: _SpyRecorder

  new iso create(recorder: _SpyRecorder) =>
    _recorder = recorder

  fun ref dispose(process: ProcessMonitor ref, s: ProcessExitStatus) =>
    _recorder.on_dispose(s)

  fun ref failed(process: ProcessMonitor ref, e: ProcessError) =>
    _recorder.on_failed()

class \nodoc\ _ExpectDisposeClient is ProcessNotify
  """
  Completes the test when `dispose` reports the expected status. Used where the
  reap arrives after a retry that outlives the seam's settle query, so the test
  must wait on `dispose` itself rather than a one-shot settle.
  """
  let _h: TestHelper
  let _expected: ProcessExitStatus

  new iso create(h: TestHelper, expected: ProcessExitStatus) =>
    _h = h
    _expected = expected

  fun ref dispose(process: ProcessMonitor ref, s: ProcessExitStatus) =>
    if s == _expected then
      _h.complete(true)
    else
      _h.fail("expected dispose(" + _expected.string() + "), got "
        + s.string())
      _h.complete(false)
    end

  fun ref failed(process: ProcessMonitor ref, e: ProcessError) =>
    _h.fail("expected dispose, got failed(" + e.error_type.string() + ")")
    _h.complete(false)

class \nodoc\ _ExpectFailedClient is ProcessNotify
  """
  Completes the test when `failed` reports the expected error type. Mirrors
  `_ExpectDisposeClient` for the reap-error path.
  """
  let _h: TestHelper
  let _expected: ProcessErrorType

  new iso create(h: TestHelper, expected: ProcessErrorType) =>
    _h = h
    _expected = expected

  fun ref failed(process: ProcessMonitor ref, e: ProcessError) =>
    if e.error_type is _expected then
      _h.complete(true)
    else
      _h.fail("expected failed(" + _expected.string() + "), got "
        + e.error_type.string())
      _h.complete(false)
    end

  fun ref dispose(process: ProcessMonitor ref, s: ProcessExitStatus) =>
    _h.fail("expected failed, got dispose(" + s.string() + ")")
    _h.complete(false)

primitive \nodoc\ _Spy
  fun monitor(h: TestHelper, recorder: _SpyRecorder,
    status: ProcessExitStatus): ProcessMonitor
  =>
    let bp_auth = ApplyReleaseBackpressureAuth(h.env.root)
    ProcessMonitor._create(bp_auth, _SpyNotify(recorder),
      recover iso _ProcessSpy(recorder, status) end,
      recover iso _Pipe.none() end, recover iso _Pipe.none() end,
      recover iso _Pipe.none() end, recover iso _Pipe.none() end)

  fun settle_then_check(
    h: TestHelper,
    pm: ProcessMonitor,
    recorder: _SpyRecorder,
    check: {(_SpyResults): (Bool, String)} val)
  =>
    // The query is sent after the driving events, so it resolves once all of
    // them have been processed by the (single-threaded) monitor actor.
    let bp = Promise[Bool]
    pm._test_query_backpressure(bp)
    bp.next[None]({(applied)(h, recorder, check) =>
      let rp = Promise[_SpyResults]
      recorder.results(applied, rp)
      rp.next[None]({(r)(h, check) =>
        (let ok, let msg) = check(r)
        if ok then
          h.complete(true)
        else
          h.fail(msg + " (" + r.string() + ")")
          h.complete(false)
        end
      })
    })

class \nodoc\ iso _TestExitReportsExactlyOnce is UnitTest
  """
  A duplicate exit event after the reap is a no-op: `dispose` fires once.
  """
  fun name(): String => "process/exit-reports-exactly-once"
  fun exclusion_group(): String => "process-monitor-seam"
  fun apply(h: TestHelper) =>
    let recorder = _SpyRecorder
    let pm = _Spy.monitor(h, recorder, Exited(3))
    pm._test_trigger_exit()
    pm._test_trigger_exit()
    _Spy.settle_then_check(h, pm, recorder, {(r: _SpyResults): (Bool, String) =>
      ((r.disposes == 1) and r.last_is(Exited(3)) and (not r.kill_after_reap)
        and (not r.backpressure_applied),
       "expected exactly one dispose of Exited(3), no kill after reap")
    })
    h.long_test(5_000_000_000)

class \nodoc\ iso _TestExitSignalRetriesUntilReapable is UnitTest
  """
  When the OS signals the child exited but waitpid still reports it running for
  a few polls, the monitor retries the reap until the status appears and reports
  the exit. Pins the retry that covers the window where waitpid trails the OS
  exit notification (the macOS EVFILT_PROC ESRCH race).
  """
  fun name(): String => "process/exit-signal-retries-until-reapable"
  fun exclusion_group(): String => "process-monitor-seam"
  fun apply(h: TestHelper) =>
    let recorder = _SpyRecorder
    let bp_auth = ApplyReleaseBackpressureAuth(h.env.root)
    // lag = 5: the probe and the first five exit-signal reaps return
    // `_StillRunning`; the reap converges only by retrying.
    let pm = ProcessMonitor._create(bp_auth,
      _ExpectDisposeClient(h, Exited(4)),
      recover iso _ProcessSpy(recorder, Exited(4), 5) end,
      recover iso _Pipe.none() end, recover iso _Pipe.none() end,
      recover iso _Pipe.none() end, recover iso _Pipe.none() end)
    pm._test_trigger_exit()
    h.long_test(5_000_000_000)

class \nodoc\ iso _TestRetryWhileDisposing is UnitTest
  """
  dispose() runs first, moving the monitor to `_Disposing`; the exit-signal reap
  then retries entirely within `_Disposing` and reports the exit once via
  `dispose`. Pins that the retry converges while the monitor is disposing.
  """
  fun name(): String => "process/retry-while-disposing"
  fun exclusion_group(): String => "process-monitor-seam"
  fun apply(h: TestHelper) =>
    let recorder = _SpyRecorder
    let bp_auth = ApplyReleaseBackpressureAuth(h.env.root)
    let pm = ProcessMonitor._create(bp_auth,
      _ExpectDisposeClient(h, Exited(0)),
      recover iso _ProcessSpy(recorder, Exited(0), 3) end,
      recover iso _Pipe.none() end, recover iso _Pipe.none() end,
      recover iso _Pipe.none() end, recover iso _Pipe.none() end)
    pm.dispose()
    pm._test_trigger_exit()
    h.long_test(5_000_000_000)

class \nodoc\ iso _TestExitSignalReapErrorReportsFailed is UnitTest
  """
  A reap that errors after the exit signal (waitpid fails once the retry finally
  polls) reports `failed(WaitpidError)`. Pins the reap-error branch on the
  exit-signal path, which the other seam tests never reach.
  """
  fun name(): String => "process/exit-signal-reap-error-reports-failed"
  fun exclusion_group(): String => "process-monitor-seam"
  fun apply(h: TestHelper) =>
    let recorder = _SpyRecorder
    let bp_auth = ApplyReleaseBackpressureAuth(h.env.root)
    // lag = 2, fail = true: the probe and two exit-signal reaps return
    // `_StillRunning`, then the reap returns `WaitpidError`.
    let pm = ProcessMonitor._create(bp_auth,
      _ExpectFailedClient(h, WaitpidError),
      recover iso _ProcessSpy(recorder, Exited(0), 2 where fail = true) end,
      recover iso _Pipe.none() end, recover iso _Pipe.none() end,
      recover iso _Pipe.none() end, recover iso _Pipe.none() end)
    pm._test_trigger_exit()
    h.long_test(5_000_000_000)

class \nodoc\ iso _TestDisposeThenExit is UnitTest
  """
  Client dispose() then the child exits: the child is killed once and the exit
  status is reported once.
  """
  fun name(): String => "process/dispose-then-exit"
  fun exclusion_group(): String => "process-monitor-seam"
  fun apply(h: TestHelper) =>
    let recorder = _SpyRecorder
    let pm = _Spy.monitor(h, recorder, Exited(0))
    pm.dispose()
    pm._test_trigger_exit()
    _Spy.settle_then_check(h, pm, recorder, {(r: _SpyResults): (Bool, String) =>
      ((r.disposes == 1) and (r.kills == 1) and (not r.kill_after_reap),
       "expected one kill, one dispose, no kill after reap")
    })
    h.long_test(5_000_000_000)

class \nodoc\ iso _TestDoubleDispose is UnitTest
  """
  Two dispose() calls kill the child once and never report an exit that has not
  happened.
  """
  fun name(): String => "process/double-dispose"
  fun exclusion_group(): String => "process-monitor-seam"
  fun apply(h: TestHelper) =>
    let recorder = _SpyRecorder
    let pm = _Spy.monitor(h, recorder, Exited(0))
    pm.dispose()
    pm.dispose()
    _Spy.settle_then_check(h, pm, recorder, {(r: _SpyResults): (Bool, String) =>
      ((r.kills == 1) and (r.disposes == 0) and (not r.backpressure_applied),
       "expected exactly one kill and no dispose")
    })
    h.long_test(5_000_000_000)

class \nodoc\ iso _TestNoKillAfterReap is UnitTest
  """
  #5765: dispose() after the child has been reaped never signals the reaped pid.
  """
  fun name(): String => "process/no-kill-after-reap"
  fun exclusion_group(): String => "process-monitor-seam"
  fun apply(h: TestHelper) =>
    let recorder = _SpyRecorder
    let pm = _Spy.monitor(h, recorder, Exited(7))
    pm._test_trigger_exit()
    pm.dispose()
    _Spy.settle_then_check(h, pm, recorder, {(r: _SpyResults): (Bool, String) =>
      ((r.kills == 0) and (not r.kill_after_reap) and (r.disposes == 1)
        and r.last_is(Exited(7)),
       "expected no kill after reap and exactly one dispose")
    })
    h.long_test(5_000_000_000)

class \nodoc\ iso _TestWriteAfterExitIsDropped is UnitTest
  """
  A write() that arrives after the child has been reaped is dropped, not
  queued: it must not fire a second dispose or strand backpressure.
  """
  fun name(): String => "process/write-after-exit-is-dropped"
  fun exclusion_group(): String => "process-monitor-seam"
  fun apply(h: TestHelper) =>
    let recorder = _SpyRecorder
    let pm = _Spy.monitor(h, recorder, Exited(0))
    pm._test_trigger_exit()
    pm.write("late data")
    pm.done_writing()
    _Spy.settle_then_check(h, pm, recorder, {(r: _SpyResults): (Bool, String) =>
      ((r.disposes == 1) and (not r.backpressure_applied),
       "a write after exit must not add a dispose or strand backpressure")
    })
    h.long_test(5_000_000_000)

// --------------------------------------------------------------------------
// The drain cap and the no-fd-leak invariant (real processes)
// --------------------------------------------------------------------------

class \nodoc\ iso _TestFloodingGrandchildDoesNotStallTeardown is UnitTest
  """
  The child backgrounds a grandchild that floods the inherited stdout without
  end, then exits. Teardown must complete and `dispose` must arrive: the child
  exits, its status is reported, and the flooding descendant is left behind (it
  gets EPIPE once we close our read end). This exercises the reap-edge drain
  against a live flooder; the `_drain_cap` is the backstop that bounds the
  drain if the reads never reach EAGAIN.
  """
  fun name(): String => "process/flooding-grandchild-does-not-stall-teardown"
  fun exclusion_group(): String => "process-monitor"
  fun apply(h: TestHelper) =>
    ifdef windows then
      h.complete(true)
    else
      let notifier =
        object iso is ProcessNotify
          fun ref failed(process: ProcessMonitor ref, err: ProcessError) =>
            h.fail("failed: " + err.string())
            h.complete(false)

          fun ref dispose(process: ProcessMonitor ref,
            status: ProcessExitStatus)
          =>
            match status
            | Exited(0) => h.complete(true)
            else
              h.fail("expected Exited(0), got " + status.string())
              h.complete(false)
            end
        end

      let process_auth = StartProcessAuth(h.env.root)
      let backpressure_auth = ApplyReleaseBackpressureAuth(h.env.root)
      let file_auth = FileAuth(h.env.root)
      let path = FilePath(file_auth, "/bin/sh")
      // cat /dev/zero floods the inherited stdout without end; the child exits
      // at once. Once we drain the cap and close our read end, cat gets EPIPE.
      let args: Array[String] val = ["sh"; "-c"; "cat /dev/zero & exit 0"]
      let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

      match StartProcess(process_auth, backpressure_auth, consume notifier,
        path, args, vars)
      | let pm: ProcessMonitor =>
        pm.done_writing()
        h.dispose_when_done(pm)
      | let err: ProcessError =>
        h.fail("StartProcess failed: " + err.string())
        h.complete(false)
      end
      h.long_test(10_000_000_000)
    end

class \nodoc\ iso _TestManyStartsDoNotLeakFds is UnitTest
  """
  Start and reap a batch of short-lived children and check that the process's
  open file descriptor count does not grow. A descriptor leaked per child
  (#5766) shows up as a count that climbs with the batch size. Linux only: it
  reads /proc/self/fd. The descriptor-closing logic is shared across posix, so
  the Linux run exercises it; the kqueue exit source on macOS and the BSDs
  holds no descriptor to leak.
  """
  fun name(): String => "process/many-starts-do-not-leak-fds"
  fun exclusion_group(): String => "process-monitor"

  fun apply(h: TestHelper) =>
    ifdef linux then
      let file_auth = FileAuth(h.env.root)
      let true_path =
        try
          _PathResolver(h.env.vars, file_auth).resolve("true")?.path
        else
          h.fail("could not find 'true' on PATH")
          h.complete(false)
          return
        end
      match _OpenFdCount(file_auth)
      | let baseline: USize =>
        _FdLeakDriver(h, 100, true_path, baseline)
      | None =>
        h.fail("could not read /proc/self/fd")
        h.complete(false)
      end
    else
      h.complete(true)
    end
    h.long_test(60_000_000_000)

primitive \nodoc\ _OpenFdCount
  """
  The number of open file descriptors in this process, read from /proc/self/fd
  on Linux. None if the directory cannot be read.
  """
  fun apply(auth: FileAuth): (USize | None) =>
    try
      let dir = Directory(FilePath(auth, "/proc/self/fd"))?
      let entries = dir.entries()?
      let n = entries.size()
      dir.dispose()
      n
    else
      None
    end

actor \nodoc\ _FdLeakDriver
  let _h: TestHelper
  let _total: USize
  let _true_path: String
  let _baseline: USize
  var _n: USize = 0

  new create(h: TestHelper, total: USize, true_path: String, baseline: USize) =>
    _h = h
    _total = total
    _true_path = true_path
    _baseline = baseline
    _start_next()

  be _done() =>
    _n = _n + 1
    if _n >= _total then
      match _OpenFdCount(FileAuth(_h.env.root))
      | let final_count: USize =>
        // A leaked descriptor per child would add _total to the count. Allow a
        // small margin for descriptors other packages' tests open while this
        // one runs in the same test binary.
        _h.assert_true(final_count <= (_baseline + 30),
          "open fd count grew from " + _baseline.string() + " to "
            + final_count.string() + " over " + _total.string() + " starts")
        _h.complete(true)
      | None =>
        _h.fail("could not read /proc/self/fd")
        _h.complete(false)
      end
    else
      _start_next()
    end

  be _fail(msg: String) =>
    _h.fail(msg)
    _h.complete(false)

  fun ref _start_next() =>
    let self: _FdLeakDriver = this
    let notifier =
      object iso is ProcessNotify
        fun ref failed(process: ProcessMonitor ref, err: ProcessError) =>
          self._fail("a start failed: " + err.string())

        fun ref dispose(process: ProcessMonitor ref,
          status: ProcessExitStatus)
        =>
          self._done()
      end

    let process_auth = StartProcessAuth(_h.env.root)
    let backpressure_auth = ApplyReleaseBackpressureAuth(_h.env.root)
    let file_auth = FileAuth(_h.env.root)
    let path = FilePath(file_auth, _true_path)
    let args: Array[String] val = ["true"]
    let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

    match StartProcess(process_auth, backpressure_auth, consume notifier, path,
      args, vars)
    | let pm: ProcessMonitor =>
      pm.done_writing()
    | let err: ProcessError =>
      _fail("a start failed: " + err.string())
    end

class \nodoc\ _FailAndExitClient is ProcessNotify
  """
  Expects an exec/chdir failure: `failed` must fire with the expected error
  type, and then `dispose` must report the child's exit. Passes only if both
  happen, so it cannot be satisfied by `dispose` alone.
  """
  let _h: TestHelper
  let _expected: ProcessErrorType
  var _failed_fired: Bool = false

  new iso create(h: TestHelper, expected: ProcessErrorType) =>
    _h = h
    _expected = expected

  fun ref stdout(process: ProcessMonitor ref, data: Array[U8] iso) =>
    None

  fun ref stderr(process: ProcessMonitor ref, data: Array[U8] iso) =>
    None

  fun ref failed(process: ProcessMonitor ref, err: ProcessError) =>
    if err.error_type is _expected then
      _failed_fired = true
    else
      _h.fail("expected " + _expected.string() + ", got "
        + err.error_type.string())
      _h.complete(false)
    end

  fun ref dispose(process: ProcessMonitor ref,
    child_exit_status: ProcessExitStatus)
  =>
    if not _failed_fired then
      _h.fail("dispose fired without a preceding failed")
      _h.complete(false)
      return
    end
    match child_exit_status
    | Exited(_EXOSERR()) => _h.complete(true)
    else
      _h.fail("expected Exited(" + _EXOSERR().string() + "), got "
        + child_exit_status.string())
      _h.complete(false)
    end
