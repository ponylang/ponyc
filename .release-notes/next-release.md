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

