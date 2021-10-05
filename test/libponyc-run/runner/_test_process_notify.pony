use "process"

class iso _TestProcessNotify is ProcessNotify
  let _tester: _Tester tag

  new iso create(tester: _Tester tag) =>
    _tester = tester

  fun ref stdout(p: ProcessMonitor ref, data: Array[U8] iso) =>
    _tester.append_stdout(consume data)

  fun ref stderr(p: ProcessMonitor ref, data: Array[U8] iso) =>
    _tester.append_stderr(consume data)

  fun ref failed(p: ProcessMonitor ref, pe: ProcessError) =>
    _tester.testing_failed()

  fun ref dispose(p: ProcessMonitor, status: ProcessExitStatus) =>
    match status
    | let exited: Exited =>
      _tester.testing_exited(exited.exit_code())
    | let signaled: Signaled =>
      _tester.testing_failed()
    end
