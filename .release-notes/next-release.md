## Fix crash when parsing, printing, or querying deeply nested JSON

Parsing, printing, or querying a deeply nested JSON value — for example a document of many thousands of nested arrays or objects — could overflow the stack and crash the program with a segfault, rather than returning a value or a catchable `JsonParseError`.

The `json` package now handles arbitrarily deep nesting, limited only by available memory.

## Reject an out-of-range `--ponygcinitial` exponent

`--ponygcinitial` sets the heap size at which an actor first garbage collects, given as the exponent `N` in a `2^N` byte threshold (the default is `2^14`, or 16 KiB). Passing a value of 64 or greater on a 64-bit platform — for instance mistaking the flag for a byte count and passing `--ponygcinitial 65536` — used to silently wrap around to a 1-byte threshold, making the actor garbage collect on nearly every allocation: the exact opposite of the large threshold the value implied. The runtime now rejects such a value with an error at startup instead of running with a wildly wrong threshold.

## Restore the message send merging optimization

A compiler optimization that batches the garbage-collection bookkeeping for consecutive message sends had been silently disabled, so it never ran. It now runs again. Code that sends several messages in a row — especially bursts to the same actor — generates fewer garbage-collection messages at runtime, lowering overhead with no change to behavior.

## Fix the start position reported for an empty JSON container's end token

`JsonTokenParser` reports a `token_start()` and `token_end()` byte offset for each token it emits. For the end token of an object or array — `JsonTokenObjectEnd` or `JsonTokenArrayEnd` — `token_start()` marks where the closing `}` or `]` begins. For an empty container it used to report the position of the opening bracket instead, so `token_start() .. token_end()` spanned the whole `{}` or `[]` rather than just the closing bracket. It now reports the closing bracket, the same as a non-empty container.

