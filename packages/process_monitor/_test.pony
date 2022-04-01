use "pony_test"
use "backpressure"
use "capsicum"
use "collections"
use "files"
use "itertools"
use "time"
use "signals"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // Tests below function across all systems and are listed alphabetically
    test(_TestBadChdir)

class \nodoc\ _TestBadChdir is UnitTest
  fun name(): String =>
    "process/BadChdir"

  fun exclusion_group(): String =>
    "process-monitor"

  fun ref apply(h: TestHelper) =>
    let badpath = Path.abs(Path.random(10))
    let notifier: ProcessNotify iso = _NoStartNotify(h)

    let process_auth = StartProcessAuth(h.env.root)
    let file_auth = FileAuth(h.env.root)

    let path = FilePath(file_auth, _PwdPath())
    let args: Array[String] val = _PwdArgs()
    let vars: Array[String] iso = recover Array[String](0) end

    try
      ProcessMonitorCreator(
        process_auth,
        consume notifier,
        path,
        args,
        consume vars,
        FilePath(file_auth, badpath))?

      h.fail()
    else
      // test passed
      None
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

  fun ref dispose(
    process: ProcessMonitor ref,
    child_exit_status: ProcessExitStatus)
  =>
    _h.fail("dispose shouldn't be called")

primitive \nodoc\ _PwdPath
  fun apply(): String =>
    ifdef windows then
      "C:\\Windows\\System32\\cmd.exe" // Have to use "cmd /c cd"
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
