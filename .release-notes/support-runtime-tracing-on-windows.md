## Support runtime tracing on Windows

The `runtime_tracing` build option now works on Windows. Previously it was a compile error on Windows, so runtime tracing was only available on Linux and macOS.

Both tracing modes are supported: writing trace events to a file as they happen, and the flight recorder, which keeps a rolling buffer of recent events in memory and dumps it, along with a backtrace and the crashing actor's state, when the program hits a fatal fault. On Windows the flight recorder also catches structured exceptions such as access violations, so a native crash triggers a dump the same way an abort does.
