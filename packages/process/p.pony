/*trait val _ProcessMonitorStateHandler
  fun ref dispose(pm: ProcessMonitor ref)

primitive _ProcessNotStarted is _ProcessMonitorStateHandler
  fun ref dispose(pm: ProcessMonitor ref) =>
    None

primitive _ProcessFailed is _ProcessMonitorStateHandler
  fun ref dispose(pm: ProcessMonitor ref) =>
    pm._release_backpressure()
    pm._close()

primitive _ProcessRunning is _ProcessMonitorStateHandler
  fun ref dispose(pm: ProcessMonitor ref) =>
    pm._release_backpressure()
    pm._kill_child()
    pm._close()

primitive _ProcessFinished is _ProcessMonitorStateHandler
  fun ref dispose(pm: ProcessMonitor ref) =>
    pm._release_backpressure()*/

