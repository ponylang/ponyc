## Fix potential use-after-free on Windows during socket teardown

When a transient system error occurred during socket teardown on Windows, the runtime freed memory that its I/O thread could still reference. The runtime now retries the operation and, if the error persists, leaves the memory allocated rather than freeing it while references remain.

