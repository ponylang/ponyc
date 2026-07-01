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

