## Fix TCPListener accept loop spin on persistent errors

The `pony_os_accept` FFI function returns a signed `int`, but `TCPListener` declared the return type as `U32`. When `accept` returned `-1` to signal a persistent error (e.g., EMFILE — out of file descriptors), the accept loop treated it as "try again" and spun indefinitely, starving other actors of CPU time.

The FFI declaration now correctly uses `I32`, and the accept loop bails out on `-1` instead of retrying. The ASIO event will re-notify the listener when the socket becomes readable, so no connections are lost.
