## Clean up child process exits on Windows

Fixed the `ProcessMonitor` class sometimes waiting on an invalid process handle on Windows.

## Fixed Windows TCP faulty connection error

Fixed a bug where a `TCPConnection` could connect to a socket that wasn't listening.

## Fixed zombie Windows TCP connection error

Fixed a bug where socket events were sometimes never unsubscribed to which would lead to "zombie" connections that were open but couldn't receive data.

## Fixed "oversleeping" on Windows

Previously, we were calling `Sleep` rather than `SleepEx` when putting scheduler threads to sleep. This could have caused Windows IO events to be missed.

## Fix broken libponyc-standalone.a

When we switched to LLVM 12, we accidentally picked up zlib being required to link against libponyc-standalone.a. That additional dependency makes the standalone library not so standalone.

We've fixed our LLVM configuration so that zlib is no longer needed.

