## Fix IOCP use-after-free crash

The fix for this issue in 0.62.0 was incomplete. That fix checked for specific Windows error codes (`ERROR_OPERATION_ABORTED` and `ERROR_NETNAME_DELETED`) in the IOCP completion callback to detect orphaned I/O operations. However, Windows can deliver completions with other error codes after the socket is closed, and `ERROR_NETNAME_DELETED` can also arrive from legitimate remote peer disconnects — making error-code matching the wrong approach entirely.

The new fix addresses the root cause: IOCP completion callbacks can fire on Windows thread pool threads after the owning actor has destroyed the ASIO event via `pony_asio_event_destroy`, leaving the callback with a dangling pointer to freed memory.

Each ASIO event now allocates a small shared liveness token (`iocp_token_t`) containing an atomic dead flag and a reference count. Every in-flight IOCP operation holds a pointer to the token and increments the reference count. When `pony_asio_event_destroy` runs, it sets the dead flag (release store) before freeing the event. Completion callbacks check the dead flag (acquire load) before touching the event — if dead, they clean up the IOCP operation struct without accessing the freed event. The last callback to decrement the reference count to zero frees the token.

This correctly handles all error codes and all IOCP operation types (connect, accept, send, recv) without swallowing events the actor needs to see.
