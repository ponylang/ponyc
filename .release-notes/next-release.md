## Fix crash when parsing, printing, or querying deeply nested JSON

Parsing, printing, or querying a deeply nested JSON value — for example a document of many thousands of nested arrays or objects — could overflow the stack and crash the program with a segfault, rather than returning a value or a catchable `JsonParseError`.

The `json` package now handles arbitrarily deep nesting, limited only by available memory.

## Reject an out-of-range `--ponygcinitial` exponent

`--ponygcinitial` sets the heap size at which an actor first garbage collects, given as the exponent `N` in a `2^N` byte threshold (the default is `2^14`, or 16 KiB). Passing a value of 64 or greater on a 64-bit platform — for instance mistaking the flag for a byte count and passing `--ponygcinitial 65536` — used to silently wrap around to a 1-byte threshold, making the actor garbage collect on nearly every allocation: the exact opposite of the large threshold the value implied. The runtime now rejects such a value with an error at startup instead of running with a wildly wrong threshold.

## Restore the message send merging optimization

A compiler optimization that batches the garbage-collection bookkeeping for consecutive message sends had been silently disabled, so it never ran. It now runs again. Code that sends several messages in a row — especially bursts to the same actor — generates fewer garbage-collection messages at runtime, lowering overhead with no change to behavior.

## Fix the start position reported for an empty JSON container's end token

`JsonTokenParser` reports a `token_start()` and `token_end()` byte offset for each token it emits. For the end token of an object or array — `JsonTokenObjectEnd` or `JsonTokenArrayEnd` — `token_start()` marks where the closing `}` or `]` begins. For an empty container it used to report the position of the opening bracket instead, so `token_start() .. token_end()` spanned the whole `{}` or `[]` rather than just the closing bracket. It now reports the closing bracket, the same as a non-empty container.

## Fix the start position reported for a JSON String or Key token

`JsonTokenParser` reports a `token_start()` and `token_end()` byte offset for each token it emits. For a `String` or `Key` token, `token_start()` now marks the opening `"`, so the reported span covers the whole quoted token. Previously it pointed one byte past the opening quote, so the span dropped the opening quote — inconsistent with every other token, which already reported its first byte.

## Stop installing the Pony runtime's C headers and static libraries

On Linux, macOS, and the BSDs, `make install` used to place ponyc's C runtime headers (`pony.h`, `paths.h`, `threads.h`, and a few others) into `<prefix>/include` and its runtime static libraries (`libponyrt.a` and friends) into `<prefix>/lib`, where `<prefix>` defaults to `/usr/local`. These were only there to support embedding the Pony runtime in a non-Pony C program by hand, which Pony does not support.

They also caused a real problem. The installed `paths.h` shadowed the system's own `<paths.h>`, and `threads.h` shadowed the C11 `<threads.h>`, so an unrelated C program that included one of those system headers could pick up Pony's by mistake.

ponyc no longer installs those headers or libraries into the shared directories. If you have a from-source install from an earlier version, your next `make install` or `make uninstall` clears out the old symlinks — only its own symlinks, never a file you put there yourself. (A prior install done with a custom `ponydir` isn't recognized by the cleanup; remove its stale `<prefix>/include/*.h` and `<prefix>/lib/libponyrt*` symlinks by hand.)

Writing C shims for your Pony code is unaffected: ponyc hands its headers to the shim compiler directly, so `#include <pony.h>` and `#include <ponyassert.h>` (for `pony_assert`) keep working with no setup. The `ponyc` compiler, the `pony-lsp`/`pony-lint`/`pony-doc` tools, and linking against `libponyc` to use the compiler as a library are all unaffected.

## Fix systematic testing being much slower than it should be

Programs run under systematic testing could run far slower than the amount of work warranted — slow enough, especially with a small number of scheduler threads, to look like they had hung. This has been fixed.

## Fix runtime stats build options on Windows

The `runtimestats` and `runtimestats_messages` build options failed to compile on Windows, so you couldn't build the runtime with runtime statistics enabled there. Both options now build on Windows.

## Fix pooltrack build option on Windows

The `pooltrack` build option failed to compile on Windows, so you couldn't build the runtime with pool allocation tracking enabled there. It now builds on Windows.

## Fix delivery of zero-byte UDP datagrams

UDP sockets now deliver zero-byte datagrams to `UDPNotify.received` with an empty payload instead of treating them as an error and tearing the socket down. A zero-length datagram is valid per RFC 768, and applications that use them as heartbeats, keepalives, or presence pings will now receive them like any other datagram. TCP zero-byte read handling, where a zero-byte read means the peer closed, is unchanged.

## Don't run the ASIO thread under systematic testing

Systematic testing (`use=systematic_testing`) replays a single scheduler interleaving from a seed so that a run is deterministic and reproducible. The ASIO thread — which handles sockets, standard input, process I/O, signals, and timers — is a real operating-system thread doing external work that a seed cannot control, so its presence left those runs only partly deterministic. A program that used any of that I/O under systematic testing would run anyway, carrying a source of nondeterminism the seed could not pin down.

Systematic testing no longer runs the ASIO thread. Any attempt to register an I/O event — opening a socket, reading standard input, spawning a process, installing a signal handler, or arming a timer — now aborts with a message saying I/O is not available under systematic testing. This is deliberate: rather than silently dropping the I/O and letting a program appear to be tested deterministically when it is not, the runtime says so plainly.

This removes the ASIO thread as a source of nondeterminism. It does not make every program deterministic on its own — a program can still introduce nondeterminism by other means, such as reading the wall clock through the FFI — and avoiding those remains the programmer's responsibility when a fully reproducible run is the goal.

## Fix executables failing to start on Linux systems with both glibc and musl

On Linux systems that have both the GNU (glibc) and musl C libraries installed, `ponyc` could build a program that was linked against one C library but set up to load the other at startup. Compilation succeeded, but the program failed the moment you ran it, printing messages such as `Error loading shared library` or `symbol not found`.

`ponyc` now selects the startup loader that matches the C library your program is linked against. If you pass an explicit `--triple`, `ponyc` honors your choice instead of guessing from what is installed on the build machine.

## Fix json package emitting invalid JSON for non-finite floating-point values

The `json` package serialized a non-finite floating-point number — infinity or NaN — as the bare word `inf`, `-inf`, `nan`, or `-nan`. None of those are valid JSON, so the output could not be parsed back, including by the package's own parser. A non-finite value now serializes as `null`.

```pony
let inf = F64(1e308) * F64(10)
JsonPrinter.print(JsonObject.update("v", inf))
// before: {"v":inf}   (not valid JSON)
// after:  {"v":null}
```

Relatedly, parsing a numeric literal outside `F64` range — such as `1e999`, or an integer of several hundred digits — used to succeed, producing one of these non-finite values. Such a literal is now rejected as a parse error.

```pony
JsonParser.parse("1e999")
// before: a non-finite F64 that cannot be serialized back to valid JSON
// after:  a JsonParseError, "Number out of range"
```

Number parsing is also more accurate at the extremes: a literal that is mathematically zero but written with a large exponent (`0e309`) previously parsed to NaN, and some in-range literals with very long digit runs parsed to infinity or a slightly wrong value — these now parse correctly.

## Add `--version` and `--help` support to pony-lsp

`pony-lsp --version` used to print nothing and hang: it ignored the flag, started the language server, and waited for input that never came. `pony-lsp` now handles `--version` and `--help` and exits, the same as `ponyc`, `pony-doc`, and `pony-lint`.

## pony-lsp now rejects unrecognized command-line arguments

`pony-lsp` used to ignore everything on its command line and start the language server regardless. It now parses its arguments the same way `ponyc`, `pony-doc`, and `pony-lint` do — it recognizes `--version` and `--help` — and rejects anything else instead of ignoring it.

This means an editor configured to launch `pony-lsp` with an argument will now fail to start the server. The Helix configuration previously published on the Pony website passed a single empty-string argument:

```toml
[language-server.pony-lsp]
command = "pony-lsp"
args = [""]
```

Remove the argument so `args` is an empty list:

```toml
[language-server.pony-lsp]
command = "pony-lsp"
args = []
```

## Fix `Sig.usr2()` reporting SIGUSR2 availability backwards

`Sig.usr2()` in the `signals` package had its availability reversed with respect to how the Pony runtime uses SIGUSR2, so it was unusable in every configuration.

On the default Linux and BSD builds, the runtime reserves SIGUSR2 for its own scheduler use, so a handler registered for it never fires — yet `Sig.usr2()` compiled and returned a signal number, silently handing you a handler that could never run. On macOS (and other `scheduler_scaling_pthreads` builds), the runtime leaves SIGUSR2 free, but `Sig.usr2()` was a compile error claiming the signal was reserved.

`Sig.usr2()` now returns a signal number on `scheduler_scaling_pthreads` builds, including macOS where that mode is always on, and is a compile error on the default builds where the runtime reserves the signal — so a misuse shows up when you compile rather than as a handler that silently never fires. One configuration is still not covered: `runtime_tracing` builds use SIGUSR2 themselves regardless of scheduler mode, and `Sig.usr2()` has no way to detect that, so it should not be used on a `runtime_tracing` build.

## Fix systematic testing occasionally hanging on Windows

On Windows, a program built with `use=systematic_testing` could occasionally hang instead of finishing. The run would stop making progress and never exit, most often as it was shutting down, and it only happened with more than one scheduler thread. Running the same program again would usually complete normally. This has been fixed.

## Fix json package losing precision when serializing floating-point values

The `json` package serialized a floating-point number with only six significant digits, so the output did not represent the original value. Parsing that output back — including with the package's own parser — returned a different number, so the package could not round-trip its own floats. A float now serializes with enough precision to read back as exactly the same `F64`, while values that need fewer digits still print compactly.

```pony
JsonPrinter.print(F64(3.141592653589793))
// before: 3.14159             (a different number)
// after:  3.141592653589793
```

## Fix TCP connections hanging on Windows

On Windows, a busy TCP connection could hang: one that had several writes queued up would, after sending its data, stop receiving and never finish -- the connection stayed stuck and the program sat idle. This was most likely on connections moving a lot of data at once. Affected connections now continue and complete normally.

## Fix `term` mishandling of unrecognized escape sequences

The `term` package's input decoder mishandled terminal escape sequences it didn't recognize. An unrecognized sequence such as Shift-Tab (`ESC[Z`) leaked stray characters into the input; an unknown keypad code stalled the decoder so following keystrokes were dropped or garbled; and an out-of-range numeric parameter could be read as a different, valid key.

The decoder now reads each escape sequence through to its end and discards it when it isn't recognized, so an unrecognized key no longer inserts stray text, stalls input, or fires the wrong key. Recognized keys are unaffected.

## Fix `term` leaving deleted text on screen after ctrl-k

In a `Readline` prompt, ctrl-k deletes from the cursor to the end of the line, but the line was not redrawn afterward, so the deleted text stayed on screen until the next keystroke that happened to redraw the line. ctrl-k now redraws the line, so the deleted text disappears right away.

## Fix Windows link failure for `ProcessSocketNotifications`

On Windows, compiling a program could fail at link time with `undefined symbol: __declspec(dllimport) ProcessSocketNotifications`, even a program that does no networking. The runtime's socket backend calls that Winsock function, and some Windows SDKs do not provide it from the system libraries ponyc was linking. ponyc now also links `mswsock.lib`, the import library for that symbol, so it resolves.

`ProcessSocketNotifications` was introduced in the Windows SDK for build 20348 (Windows 11 / Windows Server 2022) and does not exist in older SDKs. The runtime requires it, so ponyc now checks the installed SDK version and, if it is older than 10.0.20348.0, stops with a clear message telling you to install a newer SDK — instead of failing later with a confusing linker error.

## Fix `ANSITerm` not fully cleaning up on dispose

Disposing an `ANSITerm` did not fully shut it down: its `prompt` and `size` behaviors still called the notifier afterward, and the terminal-resize handler it installs was never removed, so it kept calling the closed notifier on a resize and held its signal subscription. A disposed `ANSITerm` now ignores `prompt` and `size` and removes its resize handler.

## Fix `Readline` nudging the cursor right on an empty line

When a `Readline` had an empty prompt and an empty line, refreshing the line moved the cursor one column to the right of where it belonged. It now stays at the left edge.

## Fix `Readline.down` underflow on empty history

Pressing down in a `Readline` with no history triggered an integer underflow and an out-of-bounds history access. Both are now avoided.

## Support runtime tracing on Windows

The `runtime_tracing` build option now works on Windows. Previously it was a compile error on Windows, so runtime tracing was only available on Linux and macOS.

Both tracing modes are supported: writing trace events to a file as they happen, and the flight recorder, which keeps a rolling buffer of recent events in memory and dumps it, along with a backtrace and the crashing actor's state, when the program hits a fatal fault. On Windows the flight recorder also catches structured exceptions such as access violations, so a native crash triggers a dump the same way an abort does.

## Add `rewind()` to `ArrayKeys` and `ArrayPairs`

Replicate the `fun ref rewind()` functionality from `ArrayValues` to `ArrayKeys` and `ArrayPairs`.
This enables reuse of the same `ArrayKeys` or `ArrayPairs` iterator without creating a new one.
The main use case is when an iterator is passed down to other scopes, and you don't want to pass around
references to the source array just to rewind the iterator.

Example:

```ponyc
let array: Array[USize] = recover [as F32: 1;2;3;4;5] end
let keys = array.keys()
for key in keys do
  // your code here
end

// rewind the keys iterator
keys.rewind()

// perform another iteration
for key in keys do
  // your code here
end
```

## Stop a socket read on a blocking file descriptor from hanging a program

A read on a socket could park a scheduler thread inside the read call, waiting for data that might never arrive. The runtime then never went idle and the program never exited.

The sockets `TCPConnection` and `UDPSocket` create are never in blocking mode, so this took a file descriptor from somewhere else: one opened over the C FFI, or one belonging to a library that closes and reuses descriptors itself, where a read for a closed connection can land on a descriptor number that a blocking socket has since taken over.

On Linux, macOS, and the BSDs, a socket read no longer waits for data to arrive, whatever mode the file descriptor is in. Windows offers no way to ask for that per read, so reads there still depend on the socket being non-blocking.


## Stop a socket write on a blocking file descriptor from hanging a program

A write to a socket could park a scheduler thread inside the write call, waiting for room in a send buffer that might never drain. The runtime then never went idle and the program never exited.

The sockets `TCPConnection` and `UDPSocket` create are never in blocking mode, so this took a file descriptor from somewhere else: one opened over the C FFI, or one belonging to a library that closes and reuses descriptors itself, where a write for a closed connection can land on a descriptor number that a blocking socket has since taken over.

On Linux, FreeBSD, OpenBSD, and DragonFly BSD, a socket write no longer waits for room in the send buffer, whatever mode the file descriptor is in. macOS and Windows offer no way to ask for that per write: macOS ignores the flag that asks for it on a write, though not on a read, and Windows has no such flag at all. Writes on those two still depend on the socket being non-blocking, as every socket the runtime creates is.

## Add `pony_os_sendv` for writing to a socket

`pony_os_sendv` sends an array of buffers over a socket in a single call. It is what `TCPConnection` now uses to write.

```pony
use @pony_os_sendv[U8](ev: AsioEventID,
  iov: Pointer[(Pointer[U8] tag, USize)] tag, iovcnt: I32,
  count_out: Pointer[USize])
```

It returns the same three values `pony_os_writev` does — 0 when the write completed, 1 to retry later, 2 on an error — and writes the number of bytes sent through `count_out`.

If you call `pony_os_writev` over the FFI to write to a socket, switch to `pony_os_sendv`. `pony_os_writev` is unchanged and still exported, but on Linux, macOS, and the BSDs it calls `writev`, which takes no flags argument, so nothing can request a non-blocking write: on a blocking file descriptor it parks the calling thread. `pony_os_sendv` passes `MSG_DONTWAIT` and returns a retry instead, on every platform whose kernel honors that flag on a write. In exchange it takes a socket and nothing else, where `pony_os_writev` on those platforms takes any file descriptor.
## Remove the rest of the library-mode runtime surface

Pony's library mode — embedding the runtime in a C host and stepping it by hand — was removed in earlier releases. This release removes the runtime C API functions that were left behind and that only library mode used.

These functions are gone: `pony_poll`, `pony_alloc_msg_size`, `pony_gc_acquire`, `pony_gc_release`, `pony_acquire_done`, `pony_release_done`, `pony_send`, `pony_sendp`, `pony_sendi`, `pony_register_thread`, and `pony_unregister_thread`.

Normal Pony programs are unaffected; the compiler never generated calls to any of these functions. Only C code that linked the runtime directly and called them is affected, and that code was using library mode, which is no longer supported.

