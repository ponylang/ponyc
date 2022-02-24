trait _ProcessMonitorState
  fun ref dispose(pm: ProcessMonitor)

class _ProcessNotStarted is _ProcessMonitorState
  fun ref dispose(pm: ProcessMonitor) =>
    None

class _ProcessFailed is _ProcessMonitorState
  fun ref dispose(pm: ProcessMonitor) =>
    pm._release_backpressure()

class _ProcessRunning is _ProcessMonitorState
  fun ref dispose(pm: ProcessMonitor) =>
    _child.kill()
    pm._release_backpressure()

class _ProcessFinished is _ProcessMonitorState
  fun ref dispose(pm: ProcessMonitor) =>
    pm._release_backpressure()

