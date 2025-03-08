## Fix race condition in epoll ASIO system

We've fixed a race condition in the epoll ASIO subsystem that could result in "unexpected behavior" when using one-shot epoll events. At the time the bug was fixed, this means that only the `TCPConnection` actor was impacted part of the standard library.
