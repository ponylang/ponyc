## Fix failed initialization bug in Process Monitor

Previously, when a process failed to start up, it was possible to get a report
that the process had started up and exited with the error code 0 which
normally signals success.
