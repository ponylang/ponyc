## Disable incorrect runtime assert for ASIO thread shutdown.

In a rare race condition, a runtime assertion failure related to destruction of the runtime ASIO thread's message queue could be observed during program termination, causing a program crash instead of graceful termination.

This change removes the invalid runtime assertion for that case, because the invariant it was meant to ensure does not apply or hold for the specific case of terminating the runtime ASIO thread, even though it still holds and is still being tested for the more common case of individual actor terminator.
