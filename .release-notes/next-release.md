## Fix systematic testing build failure under gcc

Building with `use=systematic_testing` under gcc failed to compile with a spurious compiler error. The build now compiles cleanly under gcc.

## Fix an intermittent systematic testing hang

Programs run under systematic testing could intermittently hang instead of running to completion. This has been fixed.

## Replace Windows IOCP socket I/O with readiness notifications

Windows networking no longer uses I/O completion ports (IOCP). The runtime now performs socket I/O with readiness notifications via the Winsock `ProcessSocketNotifications` API — the same model the Linux (epoll) and macOS/BSD (kqueue) backends already use. If you run networked Pony on Windows, the entire socket I/O path beneath `TCPConnection`, `TCPListener`, and `UDPSocket` has changed, even though your own code does not need to.

Everything else below follows from this change: real TCP write backpressure, surfaced UDP send errors, muted-connection close detection that matches the other platforms, and a new Windows version floor. It also closes a memory-safety hole. Under the old completion-based model the kernel wrote into Pony buffers after the call that started the operation returned, which no reference capability could describe; reads and writes are now synchronous, so nothing the kernel touches escapes a capability.

### Drop support for Windows 10

Because `ProcessSocketNotifications` exists only on Windows 11 and Windows Server 2022 (build 20348) and later, Windows 10 is no longer supported, and there is no fallback: a binary built with this release will fail to load on Windows 10. The supported Windows floor is now Windows 11 / Windows Server 2022.

### Windows TCP write backpressure now reflects real socket writability

On Windows, `TCPConnection` previously decided when to apply write backpressure using a heuristic based on the number of writes in flight, rather than the actual state of the socket. `throttled` and `unthrottled` now fire based on real socket writability as reported by the operating system, the same as on Linux, macOS, and BSD.

### Windows UDP send failures are no longer silently discarded

On Windows, a failed `UDPSocket` send previously produced no error and no notification at all. A send that fails now surfaces the error and closes the socket, delivering `closed()` to the `UDPNotify`, matching the behavior on other platforms. (A send that would merely block on a non-blocking socket is still silently dropped, as it is on every platform.)

### Windows muted TCPConnection no longer detects peer close until unmuted

On Windows, a muted `TCPConnection` previously still noticed when its peer closed the connection. It now behaves like every other platform: while muted, a connection does not learn that its peer has closed until it is unmuted. As on all platforms, you **must** call `unmute` on a muted connection for it to close — without it the `TCPConnection` actor will never exit.

## Improve memory usage

A program could use a large and growing amount of memory when an actor repeatedly forwarded an object it had received from another actor back to itself. Such programs now run in bounded memory.

## Remove support for Alpine 3.21

We no longer test against it or build ponyc releases for it.

## Remove support for Alpine 3.22

We no longer test against it or build ponyc releases for it.

