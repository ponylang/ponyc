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

## Add a streaming JSON parser

`JsonTokenParser` now parses JSON incrementally, so you can parse input that arrives in pieces — over a socket, or a large file read in chunks — or that is too big to hold in memory at once. Feed it bytes with `feed()` as they arrive and it pushes tokens to your notifier as they complete, building no tree, so you control how much memory the parse uses: process each token and drop it and memory stays flat however large the document.

```pony
let parser = JsonTokenParser(notify)
parser.feed(chunk)?  // once per chunk, as bytes arrive
parser.finish()?     // when the input ends
```

When a string or key value fits within a single fed chunk with no escapes, it is a zero-copy view into that chunk rather than a fresh copy. `JsonReassembler` folds a token stream back into the same `JsonValue` the batch `JsonParser` builds, when you want one, and `JsonParseLimits` caps nesting depth and string and number length for untrusted input. `JsonParser.parse()` is unchanged.

## Change JsonTokenParser to carry values on its tokens

This is a breaking change to `JsonTokenParser`. Its tokens now carry their own value instead of exposing it through `parser.last_string` / `parser.last_number`, and it is driven with `feed()` / `finish()` instead of `parse()`.

Before:

```pony
let parser = JsonTokenParser(
  object is JsonTokenNotify
    fun ref apply(p: JsonTokenParser, token: JsonToken) =>
      match token
      | JsonTokenKey => use_key(p.last_string)
      | JsonTokenNumber =>
        match p.last_number
        | let i: I64 => use_int(i)
        | let f: F64 => use_float(f)
        end
      end
  end)
parser.parse(whole_document)?
```

After:

```pony
let parser = JsonTokenParser(
  object is JsonTokenNotify
    fun ref apply(p: JsonTokenParser, token: JsonToken) =>
      match token
      | let k: JsonTokenKey => use_key(k.value)
      | let n: JsonTokenNumber =>
        match n.value
        | let i: I64 => use_int(i)
        | let f: F64 => use_float(f)
        end
      end
  end)
parser.feed(whole_document)?
parser.finish()?
```

`JsonParser.parse()` is unchanged.
