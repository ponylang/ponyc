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

## Enforce documented bounds for --ponycdinterval

The help text for `--ponycdinterval` has always said the minimum is 10 ms and the maximum is 1000 ms, but the runtime never actually enforced either bound on the command line. You could pass any non-negative value and it would be silently clamped deep in the cycle detector initialization. Values above ~2147 would also overflow during an internal conversion to CPU cycles, producing nonsensical detection intervals.

The documented bounds are now enforced. Passing a value outside [10, 1000] on the command line will produce an error. Values set via `RuntimeOptions` continue to be clamped to the valid range.

## Add missing NULL checks for gen_expr results in gencall.c

Two additional code paths in the compiler could crash instead of reporting errors when a receiver sub-expression encountered a codegen error. These are the same class of bug as the recently fixed crash when calling methods on invalid shift expressions, but in the `gen_funptr` and `gen_pattern_eq` code paths. These are harder to trigger in practice but could cause segfaults in the LLVM optimizer if encountered.

## Fix type system soundness hole

The compiler incorrectly accepted aliased type parameters (`X!`) as subtypes of their unaliased form when used inside arrow types. This allowed code that could duplicate `iso` references, breaking reference capability guarantees. For example, a function could take an aliased (tag) reference and return it as its original capability (potentially iso), giving you two references to something that should be unique.

Code that relied on this — likely by accident — will now get a type error. The most common pattern affected is reading a field into a local variable and returning it from a method with a `this->A` return type:

```pony
// Before: compiled but was unsound
class Container[A]
  var inner: A
  fun get(): this->A =>
    let tmp = inner
    consume tmp

// After: return the field directly instead of going through a local
class Container[A]
  var inner: A
  fun get(): this->A => inner
```

The intermediate `let` binding auto-aliases the type to `this->A!`, and consuming doesn't undo the alias. Returning the field directly avoids the aliasing entirely.

The persistent list in the `collections/persistent` package had four signatures using `val->A!` that relied on this bug. These have been changed to `val->A`. If you had code implementing the same interfaces with explicit `val->A!` types, change them to `val->A`.

## Fix cap_isect_constraint returning incorrect capability for empty intersections

When a type parameter was constrained by an intersection of types with incompatible capabilities (e.g., `ref` and `val`), the compiler incorrectly computed the effective capability as `#any` (the universal set) instead of recognizing that no capability satisfies both constraints. This could cause the compiler to silently accept type parameter constraints that have no valid capability, rather than reporting an error.

The compiler now correctly detects empty capability intersections and reports "type parameter constraint has no valid capability" when the intersection of capabilities in a type parameter's constraint is empty. This also fixes incorrect results for `iso` intersected with `#share` and `#share` intersected with concrete capabilities outside its set, which were caused by missing `break` statements and an incorrect case in the capability intersection logic.

## Fix incorrect pool free when tidying the reachability painter

The compiler’s reachability “painter” frees internal `colour_record_t` nodes when cleaning up. Those allocations must be returned to the same memory pool they came from. A bug passed a size expression as the first argument to `POOL_FREE` instead of the type name, so the wrong pool index was used when freeing those records.

That could corrupt the allocator’s bookkeeping during compilation in scenarios that exercise that cleanup path. The free now uses the correct type, matching how other `POOL_FREE` calls work in the codebase.

