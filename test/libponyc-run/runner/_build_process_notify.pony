use "process"

class iso _BuildProcessNotify is ProcessNotify
  let _tester: _Tester tag

  new iso create(tester: _Tester tag) =>
    _tester = tester

  fun ref stdout(p: ProcessMonitor ref, data: Array[U8] iso) =>
    _tester.append_stdout(consume data)

  fun ref stderr(p: ProcessMonitor ref, data: Array[U8] iso) =>
    _tester.append_stderr(consume data)

  fun ref failed(p: ProcessMonitor ref, pe: ProcessError) =>
    _tester.building_failed("build process failed to start")

  fun ref dispose(p: ProcessMonitor, status: ProcessExitStatus) =>
    match status
    | let exited: Exited =>
      if exited.exit_code() == 0 then
        _tester.building_succeeded()
      else
        _tester.building_failed("exited with code "
          + exited.exit_code().string())
      end
    | let signaled: Signaled =>
      _tester.building_failed("signal " + signaled.signal().string())
    end
