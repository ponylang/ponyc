## Fix use-after-free in IOCP ASIO system

We fixed a pair of use-after-free races in the Windows IOCP event system. A previous fix introduced a token mechanism to prevent IOCP callbacks from accessing freed events, but missed two windows where raw pointers could outlive the event they pointed to. One was between the callback and event destruction, the other between a queued message and event destruction.

This is the hard part that Pony protects you from. Concurrent access to mutable data across threads is genuinely difficult to get right, even when you have a mechanism designed specifically to handle it.

