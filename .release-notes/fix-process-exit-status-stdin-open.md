## Fix ProcessMonitor never reporting a child's exit status when stdin is open

`ProcessMonitor` did not report a child process's exit status while the parent still held the child's stdin open. A child that exited on its own — on a quit command, on bad input, or by crashing — never triggered a `ProcessNotify.dispose` call, and the program could hang.

This ruled out keeping a long-running child that you write to and that exits on its own. Calling `done_writing()` to close stdin was the only way to receive the exit status, and that meant giving up writing to the child.

`ProcessMonitor` now calls `ProcessNotify.dispose` with the child's exit status once the child exits, whether or not you have closed its stdin.
