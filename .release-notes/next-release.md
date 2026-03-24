## Fix IOCP use-after-free crash

The fix for this issue in 0.62.0 was incomplete. That fix checked for specific Windows error codes (`ERROR_OPERATION_ABORTED` and `ERROR_NETNAME_DELETED`) in the IOCP completion callback to detect orphaned I/O operations. However, Windows can deliver completions with other error codes after the socket is closed, and `ERROR_NETNAME_DELETED` can also arrive from legitimate remote peer disconnects — making error-code matching the wrong approach entirely.

The new fix addresses the root cause: IOCP completion callbacks can fire on Windows thread pool threads after the owning actor has destroyed the ASIO event via `pony_asio_event_destroy`, leaving the callback with a dangling pointer to freed memory.

Each ASIO event now allocates a small shared liveness token (`iocp_token_t`) containing an atomic dead flag and a reference count. Every in-flight IOCP operation holds a pointer to the token and increments the reference count. When `pony_asio_event_destroy` runs, it sets the dead flag (release store) before freeing the event. Completion callbacks check the dead flag (acquire load) before touching the event — if dead, they clean up the IOCP operation struct without accessing the freed event. The last callback to decrement the reference count to zero frees the token.

This correctly handles all error codes and all IOCP operation types (connect, accept, send, recv) without swallowing events the actor needs to see.

## Fix pony-lint ignore matching on Windows

pony-lint's `.gitignore` and `.ignore` pattern matching failed on Windows because path separator handling was hardcoded to `/`. On Windows, where paths use `\`, ignore rules were silently ineffective — files that should have been skipped were linted, and anchored patterns like `src/build/` never matched. Windows CI for tool tests has been added to prevent regressions.

## Fix pony-lsp on Windows

pony-lsp's JSON-RPC initialization failed on Windows because filesystem paths containing backslashes were embedded directly into JSON strings, producing invalid escape sequences. The LSP file URI conversion also didn't handle Windows drive-letter paths correctly. Additionally, several directory-walking loops in the workspace manager and router used `Path.dir` to walk up to the filesystem root, terminating when the result was `"."` — which works on Unix but not on Windows, where `Path.dir("C:")` returns `"C:"` rather than `"."`, causing an infinite loop. Windows CI for tool tests has been added to prevent regressions.
## Enforce documented maximum for --ponysuspendthreshold

The help text for `--ponysuspendthreshold` has always said the maximum value is 1000 ms, but the runtime never actually enforced it. You could pass any value and it would be accepted. Values above ~4294 would silently overflow during an internal conversion to CPU cycles, producing nonsensical thresholds.

The documented maximum of 1000 ms is now enforced. Passing a value above 1000 on the command line will produce an error. Values set via `RuntimeOptions` are clamped to 1000.

## Fix compiler crash when calling methods on invalid shift expressions

The compiler would crash with a segmentation fault when a method call was chained onto a bit-shift expression with an oversized shift amount. For example, `y.shr(33).string()` where `y` is a `U32` would crash instead of reporting the "shift amount greater than type width" error. The shift amount error was detected internally but the crash occurred before it could be reported. Standalone shift expressions like `y.shr(33)` were not affected and correctly produced an error message.

